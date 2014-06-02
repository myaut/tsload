
'''
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.    
'''  



# Storm ORM doesn't support creating table from custom classes
# Taken from https://code.launchpad.net/~shoxi/storm/auto-schema-creation
from storm.variables import (Variable, RawStrVariable, BoolVariable,
    IntVariable, FloatVariable, DecimalVariable, UnicodeVariable,
    DateTimeVariable, DateVariable, TimeVariable, TimeDeltaVariable,
    EnumVariable, PickleVariable, ListVariable, UUIDVariable, JSONVariable)
from storm.properties import SimpleProperty
from storm.info import get_cls_info

from storm import tracer 
from tsload import logging

#=======
# SQLite

_sqliteColumnTypes = {
        RawStrVariable: 'TEXT',
        
        BoolVariable: 'BOOL',
        IntVariable: 'INTEGER',
        FloatVariable: 'REAL',
        DecimalVariable: 'TEXT',
        
        UnicodeVariable: 'TEXT',
        UUIDVariable: 'TEXT',
        JSONVariable: 'TEXT',
        EnumVariable: 'INTEGER',
        
        DateTimeVariable: 'DATETIME',
        DateVariable: 'DATE',
        TimeVariable: 'TIME',
        TimeDeltaVariable: 'TIMEDELTA',
        
        PickleVariable: 'BLOB',
        ListVariable: 'BLOB'
    }

#=======
# MySQL

def _handleMySQLEnum(enumVar):
    enumKeys = enumVar._get_map.keys()
    
    return 'ENUM (%s)' % ', '.join("'%s'" % k for k in enumKeys) 

_mysqlColumnTypes = {
        RawStrVariable: 'BINARY',
        
        BoolVariable: 'BOOL',
        IntVariable: 'INT',
        FloatVariable: 'DOUBLE',
        DecimalVariable: 'DECIMAL', # NOTE: supports up to 30 digits in decimal type 
        
        UnicodeVariable: 'VARCHAR(255)',
        UUIDVariable: 'VARCHAR(40)',
        JSONVariable: 'TEXT',
        
        DateTimeVariable: 'DATETIME',
        DateVariable: 'DATE',
        TimeVariable: 'TIME',
        TimeDeltaVariable: 'TIMEDELTA', 
        
        EnumVariable: _handleMySQLEnum,
        PickleVariable: 'BINARY',
        ListVariable: 'BINARY'
    }

_columnTypes = {'SQLite': _sqliteColumnTypes,
               'MySQL': _mysqlColumnTypes}

_autoIncrement = {'SQLite': 'AUTOINCREMENT',
                  'MySQL': 'AUTO_INCREMENT'}

_uniqueConstraint = {'SQLite': 'UNIQUE',
                     'MySQL': 'UNIQUE KEY'}

class UniqueConstraint:
    def __init__(self, *columns):
        self.columns = columns

class UniqueConstraintImpl:
    def __init__(self, name, columns):
        self.name = name
        self.columns = columns
    
    def format(self, dbClass):
        colListStr = ', '.join(col.name 
                               for col 
                               in self.columns)
        
        if dbClass == 'SQLite':
            return '%s(%s) ON CONFLICT REPLACE' % (_uniqueConstraint[dbClass], 
                                              colListStr)
        
        return '%s %s (%s)' % (_uniqueConstraint[dbClass], 
                               self.name, 
                               colListStr)

class TableSchema:
    '''Generates table schema for Storm db object class.
    Supports property attributes:
    - .default - default value for this property
    - .notNull (bool) - field value is not nullable
    - .unique (bool) - unique constraint
    - primary=True argument to property constructor'''
    
    # TODO: PostgreSQL Support?
    # TODO: indexes
    
    def __init__(self, db, cls):
        '''Construct new TableSchema
        
        @param db: reference to database object (i.e. returned by create_database)
        @param cls: db object class
        
        NOTE: uses db class name to determine database system (SQLite, MySQL, etc.)'''
        self.cls = cls
        self.dbClass = db.__class__.__name__
        
        colTypeTable = _columnTypes[self.dbClass]
        
        self.cls_info = get_cls_info(cls)
        
        self.columns = []
        self.constraints = []
        self.unique = []
        
        self.tableName = self.cls_info.table.name
        
        for var, col in cls._storm_columns.items():
            # Get column type from table
            
            # XXX: This hack relies on VariableFactory implementation that uses
            # functools.partial (see storm/variables.py). Don't know, how to do
            # it better because we loose any information about variable class
            # after get_cls_info()
            colType = colTypeTable[col.variable_factory.func]    
            if callable(colType):
                colType = colType(col)
            
            colSpec = [col.name, colType]
            
            # Add options (NOT NULL, PRIMARY KEY + AUTOINCREMENT, DEFAULT, UNIQUE)
            default = getattr(col, 'default', None)
            if default is not None:
                colSpec.extend('DEFAULT %s' % (colType, repr(default)))
                
            uniq = getattr(var, 'unique', False)
            if uniq:
                self.unique.append(col)
                
                if self.dbClass == 'SQLite':
                    colSpec.append('UNIQUE')
                else:
                    constraint = ConstraintImpl(_uniqueConstraint[self.dbClass],
                                                col.name, [col])
                    self.constraints.append(constraint)
            
            # col == pk generates SQL expression Eq, so bool(Eq) == True in any case
            # In this case, 'in' operator will return True in any case too 
            if any(pk is col 
                   for pk 
                   in self.cls_info.primary_key):
                colSpec.append('PRIMARY KEY')
                colSpec.append(_autoIncrement[self.dbClass])
            
            allowNone = var._variable_kwargs.get('allow_none', False)
            if not allowNone:
                colSpec.append('NOT NULL')
            
            self.columns.append(colSpec)
        
        for conName, con in cls.__dict__.iteritems():
            if isinstance(con, UniqueConstraint):
                constraint = UniqueConstraintImpl(conName,
                                                  [prop._get_column(cls) 
                                                   for prop 
                                                   in con.columns])
                self.constraints.append(constraint)
    
    def create(self, store, ifNotExists = False):
        '''Creates new table from scratch and saves it into store'''
        singlePrimaryKey = len(self.cls_info.primary_key) == 1
        
        columns = []
        constraints = []
        
        def _contraintFilter(spec):
            return  singlePrimaryKey or not spec == 'PRIMARY KEY'
        
        for colSpec in self.columns:
            colSpec = filter(_contraintFilter, colSpec)
            
            columns.append(' '.join(colSpec))
            
        if not singlePrimaryKey and self.cls_info.primary_key:
            primaryKeys = ', '.join(col.name 
                                    for col 
                                    in self.cls_info.primary_key)
            constraints.append('PRIMARY KEY ( ' + primaryKeys + ' )')
            
        for constraint in self.constraints:
            constraints.append(constraint.format(self.dbClass))
        
        createOption = ' IF NOT EXISTS ' if ifNotExists else '' 
        
        query = 'CREATE TABLE ' + createOption + self.tableName + ' '
        query += '( ' + ', '.join(columns + constraints) + ');'
        
        print query
        store.execute(query)
    
    def patch(self, store, callbacks = {}):
        '''Parses table description, adds new columns and removes columns
        that was removed from model''' 
        # TODO: patch functions
        
        
        pass
    
class StormDebugTracer(object):
    def __init__(self):
        pass

    def connection_raw_execute(self, connection, raw_cursor, statement, params):
        raw_params = []
        for param in params:
            if isinstance(param, Variable):
                raw_params.append(param.get())
            else:
                raw_params.append(param)
        raw_params = tuple(raw_params)
        logging.trace('storm', "EXECUTE: %r, %r\n", statement, raw_params)

    def connection_raw_execute_error(self, connection, raw_cursor,
                                     statement, params, error):
        logging.trace('storm', 'ERROR %s', error)

    def connection_raw_execute_success(self, connection, raw_cursor,
                                       statement, params):
        logging.trace('storm', 'DONE')

def initStormTracing():
    tracer.install_tracer(StormDebugTracer())
