from twisted.internet import reactor
from twisted.internet.defer import inlineCallbacks, Deferred

class ContextError(Exception):
    pass

class NextContext:
    def __init__(self, klass):
        self.klass = klass
        
    def __call__(self, func):
        def wrapper(context, *args, **kwargs):
            d = func(context, *args, **kwargs)
            return context.nextContext(self.klass), d
        wrapper.__doc__ = func.__doc__
        return wrapper
    
class ReturnContext:
    def __init__(self):
        pass
    
    def __call__(self, func):
        def wrapper(context, *args, **kwargs):
            d = func(context, *args, **kwargs)
            return context.parent, d
        wrapper.__doc__ = func.__doc__
        return wrapper
    
class SameContext:
    def __init__(self):
        pass
    
    def __call__(self, func):
        def wrapper(context, *args, **kwargs):
            d = func(context, *args, **kwargs)
            return context, d
        wrapper.__doc__ = func.__doc__
        return wrapper

class CLIContext:
    async = False
    operations = ['help', 'done', 'exit']
    
    def __init__(self, parent, cli):
        self.parent = parent
        self.cli = cli
        
    def path(self):
        path = []
        context = self
        
        while context:
            path.insert(0, context.name)
            context = context.parent
            
        return ' '.join(path)
    
    def call(self, args):
        op = args.pop(0)
        
        if op not in self.operations + CLIContext.operations:
            raise ContextError('%s: Invalid operation %s' % 
                               (self.__class__.__name__, op))
        
        try:
            method = getattr(self, op)
        except AttributeError:
            raise ContextError('%s: Unknown operation %s' % 
                               (self.__class__.__name__, op))
        
        return method(args)

    def nextContext(self, klass):
        return klass(self, self.cli)
    
    @SameContext()
    def help(self, args):
        '''Prints help'''
        for op in self.operations + CLIContext.operations:
            method = getattr(self, op)
            try:
                args = method.args
            except AttributeError:
                args = []
                
            doc = method.__doc__
            if doc is None:
                doc = ''
                
            print '%-12s %s' % (' '.join([op] + args), doc)
            print
    
    @ReturnContext()
    def done(self, args):
        '''Returns to previous context'''
        pass
    
    def doResponse(self, args):
        '''doResponse must be implemented for asynchronous contexts.
        For synchronous contexts who uses inlineCallbacks, returns itself'''
        self.async = False
        return self, args
    
    def exit(self, args):
        raise SystemExit()