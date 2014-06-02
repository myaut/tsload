
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



import crypt
import random
import string

from tsload.user import UserAuthError

class LocalAuth:
    def authentificate(self, user, password):
        '''Authentificate user using local database entry
        
        @param user: User entry in database
        @param password: Raw password string
        
        @return True if user was successfully authentificated otherwise 
        UserAuthError is raised'''
        try:
            encMethod, salt, encPassword1 = str(user.authPassword).split('$', 2)
        except ValueError:
            raise UserAuthError('Invalid password entry in database.')
        
        encPassword2 = crypt.crypt(password, salt)
        
        if encPassword1 != encPassword2:
            raise UserAuthError('You entered incorrect password')
        
        return True
        
    def changePassword(self, user, newPassword, origPassword = None):
        '''Change password or assign new one to user
        
        @param user: User entry in database
        @param newPassword: New password
        @param origPassword: Original password - optional if new password
        is assigned or administrator issues password reset'''
        if origPassword is not None:
            self.authentificate(user, origPassword)
            
        salt = random.choice(string.letters) + random.choice(string.digits)
        encPassword = crypt.crypt(newPassword, salt)
        
        user.authPassword = '%s$%s$%s' % ('crypt', salt, encPassword)
        
