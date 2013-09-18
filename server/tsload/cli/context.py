from twisted.internet import reactor
from twisted.internet.defer import inlineCallbacks, Deferred

class ContextError(Exception):
    pass

class ContextDecorator:
    def wrap(self, wrapper, func):
        wrapper.__doc__ = func.__doc__
        wrapper.decorator = self
        return wrapper
    
class NextContext(ContextDecorator):
    def __init__(self, klass):
        self.klass = klass
        
    def __call__(self, func):
        def wrapper(context, *args, **kwargs):
            d = func(context, *args, **kwargs)
            return context.nextContext(self.klass), d
        return self.wrap(wrapper, func)
    
class ReturnContext(ContextDecorator):
    def __init__(self):
        pass
    
    def __call__(self, func):
        def wrapper(context, *args, **kwargs):
            d = func(context, *args, **kwargs)
            return context.parent, d
        return self.wrap(wrapper, func)
    
class SameContext(ContextDecorator):
    def __init__(self):
        pass
    
    def __call__(self, func):
        def wrapper(context, *args, **kwargs):
            d = func(context, *args, **kwargs)
            return context, d
        return self.wrap(wrapper, func)

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

    def completer(self, text, state):
        # Search for context
        ctxClass = self.__class__
        ctxClassStack = [ctxClass]
        args = text.split()
        
        # str.split() cuts last arg, add empty op, so completer
        # will show every possible operation
        if text.endswith(' '):
            args.append('')
        
        for op in args[:-1]:
            # Invalid operation - ignore
            if op not in ctxClass.operations:
                return None
            
            try:
                method = getattr(ctxClass, op)
            except AttributeError:
                return None
            
            if isinstance(method.decorator, NextContext):
                ctxClass = method.decorator.klass                
                ctxClassStack.append(ctxClass)
            elif isinstance(method.decorator, ReturnContext):
                ctxClass = ctxClassStack.pop()
            elif isinstance(method.decorator, SameContext):
                pass
            else:
                # Unknown decorator
                return None
            
            # async contexts has no subcommands - ignore them
            if ctxClass.async:
                return None
        
        # Operation "agent load exit" is strange, so add CLIContext
        # operations only if it is first argument
        if len(args) == 1:
            operations = ctxClass.operations + CLIContext.operations
        else:
            operations = ctxClass.operations 
        
        # We have context class - run completer
        operations = filter(lambda op: op.startswith(args[-1]),
                            operations)
        
        if state < len(operations):
            return operations[state]

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