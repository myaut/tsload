'''
Created on May 13, 2013

@author: myaut
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
        