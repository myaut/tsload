from twisted.internet import reactor

class ContextError(Exception):
    pass

class NextContext:
    def __init__(self, klass):
        self.klass = klass
        
    def __call__(self, func):
        def wrapper(context, *args, **kwargs):
            ctxargs = func(context, *args, **kwargs)
            return context.nextContext(self.klass), ctxargs
        wrapper.__doc__ = func.__doc__
        return wrapper
    
class ReturnContext:
    def __init__(self):
        pass
    
    def __call__(self, func):
        def wrapper(context, *args, **kwargs):
            ctxargs = func(context, *args, **kwargs)
            return context.parent, ctxargs
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
        
        return self, []
    
    def done(self, args):
        '''Returns to previous context'''
        return self.parent, []
    
    def exit(self, args):
        reactor.stop()
        return self, []