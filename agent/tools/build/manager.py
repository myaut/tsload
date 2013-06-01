import os
from itertools import chain

class BuildItem:
    def __init__(self, targets, deps):
        self.targets = targets
        self.deps = deps
        
    def depend(self, dep):
        self.deps.append(dep)
    
    def check(self, ignore_items):
        for dep in self.deps:
            if dep in ignore_items:
                return False
        
        return True

class BuildManager:
    def __init__(self):
        self.build_items = {}
        self.build_item_names = []
        self.build_item_default = []
            
    def AddBuildItem(self, group, name, targets = [], deps = [], default=True):
        full_name = group + '-' + name
        deps = chain(deps, *[self.build_items[item].deps for item in deps])
        
        item = BuildItem(targets, list(set(deps)))
        
        self.build_item_names.append(full_name)
        
        if default:
            self.build_item_default.append(full_name)
        
        self.build_items[full_name] = item
        
        return full_name
    
    def AddModule(self, group, mod):
        mod_name = os.path.basename(str(mod))
        
        module = self.AddBuildItem(group, 'mod-' + mod_name, 
                                   targets = [str(mod)],
                                   deps = [group + '-lib'])
        self.build_items[module].depend(group + '-modules')

    def __getitem__(self, name):
        return self.build_items[name]

    def PrepareBuild(self, args):
        build_items = []
        ignore_items = []
        build_deps = []
        
        # Read arguments from args
        for arg in args:
            if arg[0] == '!':
                ignore_items.append(arg[1:])
            elif arg == 'all':
                build_items = self.build_item_names
            else:
                build_items.append(arg)
        
        # By default we build all items
        if not build_items:
            build_items = self.build_item_default
        
        # Resolve dependencies
        for item in build_items:
            build_item = self[item]
            
            if build_item.check(ignore_items):
                build_deps += build_item.deps
            else:
                ignore_items.append(item)
        
        # Remove ignore items from build items
        for item in ignore_items:
            if item in build_items:
                build_items.remove(item)
        
        # Insert all dependencies into beginning
        for item in build_deps:
            if item not in build_items:
                build_items.insert(0, item)
                
        self.build_items = [build_item 
                            for name, build_item
                            in self.build_items.items()
                            if name in build_items]
    
    def Print(self):
        for name in self.build_item_names:
            build_item = self.build_items[name]
            
            prefix = '=> ' if name in self.build_item_default else ''
            
            print prefix + name + ': ' + ' '.join(build_item.deps) 
            print '\n'.join('\t' + target 
                            for target
                            in build_item.targets)
            print
         
    def GetBuildTargets(self):
        return chain(*[build_item.targets
                       for build_item
                       in self.build_items])
        

# Singlethon
build_manager = BuildManager()

AddBuildItem = build_manager.AddBuildItem
AddModule = build_manager.AddModule
PrepareBuild = build_manager.PrepareBuild
GetBuildTargets = build_manager.GetBuildTargets
BuildManagerPrint = build_manager.Print