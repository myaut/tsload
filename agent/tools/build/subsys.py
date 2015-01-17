from ConfigParser import ConfigParser

class Subsystem(object):
    def __init__(self, name, **kwargs):
        self.name = name
        self.alias = kwargs.get('alias', name)
        
        self.command = kwargs.get('cmd')
        self.library = kwargs.get('lib')
        
        self.generic = 'generic' in kwargs
        
        parse_deps = lambda key: filter(None, kwargs.get(key, '').split(','))
        self.deps = parse_deps('deps')
        self.rdeps = parse_deps('rdeps')
    
    def __str__(self):
        return '<SubSystem %s>' % self.name

class SubsystemList(object):
    class Iterator(object):
        def __init__(self, ssl):
            self.ssl = ssl
            self.ss_iter = iter(ssl.ss_list)
        
        def __iter__(self):
            return self
        
        def next(self):
            ss_name = next(self.ss_iter)
            
            return self.ssl.subsystems[ss_name]
    
    def __init__(self, ss_list):
        self.subsystems = {}
        self.ss_list = ss_list
    
    def read(self, ss_list_path):
        cfg = ConfigParser()
        cfg.read(ss_list_path)
        
        for ss_name in cfg.sections():
            ss = Subsystem(ss_name, **dict(cfg.items(ss_name)))
            self.subsystems[ss_name] = ss
    
    def build(self):
        # Build large list containing all subsystems and its dependencies
        # This is O(n * log n) algorithm, but it is much simpler than
        # topological sorting or some kind of it... 
        
        ss_list = []
        
        def add_subsystem(ss_name):
            if ss_name in ss_list:
                return 
            
            ss = self.subsystems[ss_name]
            
            for dep in ss.deps:
                if dep not in ss_list:
                    add_subsystem(dep)
            
            if not ss.generic:
                ss_list.append(ss_name)
            
            for rdep in ss.rdeps:
                # Move reverse dependency forward
                if rdep in ss_list:
                    ss_list.remove(rdep)
                add_subsystem(rdep)
        
        for ss_name in self.ss_list:
            add_subsystem(ss_name)
        
        self.ss_list = ss_list
    
    def __iter__(self):
        return SubsystemList.Iterator(self)
            