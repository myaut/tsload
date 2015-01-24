## modinfo.json

modinfo.json file is a configuration file that is used by [tsgenmodsrc][ref/tsgenmodsrc] for generating source code of modules. 

This document uses same JSON schema that is used in [experiment.json][ref/experiment_json].

### modinfo.json

```
{
	(in) "name": [string] Name of module,
	(in, opt) "type": ["load"] Type of the module, currently only "load" supported,
	(in) "wltypes": [node] {
		(in) "Workload type name #1": [node] Workload type,
		...
	},
	(in) "vars": [node] {
		(in) "Variable name #1": [string] Value,
		...
	}
}
```

### Workload type

```
{
	(in) "class": [
		(in) ["cpu_integer" | "cpu_float" | "cpu_memory" | "cpu_misc" | 
		 		"mem_allocation" | "fs_op" | "fs_rw" | "disk_rw" | "network" | "os"]
		 		Classes that this workload implements		 
		...
	],
	(in, opt) "has_step": [boolean] Set to true if you want step function to be generated,	
	(in, opt) "has_data": [boolean] Set to true if you want data structure to be generated,	
	(in) "params": [node] {
		(in) "Name of parameter #1": [node] Parameter,
		...
	},
	(in) "vars": [node] {
		(in) "Variable name #1": [string] Value,
		...
	}
}
```

### Parameters

```
{
	(in) "_type": [string] Data type of a parameter,
	
	(in, opt) "request": [boolean] Parameter is per-request,
	(in, opt) "output": [boolean] Parameter is output,
	(in, opt) "optional": [boolean] Parameter is optional,
	
	(in, opt) "default": [ANY] Default value of a parameter
					(not supported by CPU objects and disks),
	(in, opt, min is set) "min": [ANY] Minimum acceptable value
	(in, opt, max is set) "max": [ANY] Maximum acceptable value
					(only for floats, integers and integer-based types),
	(in, opt) "len": [number] Maximum length of string,
	
	(in) "strset" : [array] [
			Value # 1...
		]	
}
```

`_type` field contains pseudo-class name of a corresponding parameter. Here are a table of mappings of this field values to `wlp_type_t` constants (as described in [Key concepts: workloads][intro/workload])

---
wlp\_type\_t     | \_type value
WLP_BOOL         | tsload.wlparam.WLParamBoolean
WLP_INTEGER      | tsload.wlparam.WLParamInteger
WLP_FLOAT        | tsload.wlparam.WLParamFloat
WLP_RAW_STRING   | tsload.wlparam.WLParamString
WLP_STRING_SET   | tsload.wlparam.WLParamStringSet
WLP_SIZE         | tsload.wlparam.WLParamSize
WLP_TIME         | tsload.wlparam.WLParamTime
WLP_FILE_PATH    | tsload.wlparam.WLParamFilePath
WLP_CPU_OBJECT   | tsload.wlparam.WLParamCPUObject
WLP_DISK         | tsload.wlparam.WLParamDisk
---
Note: this format is conformant with data returned by `tsexperiment wls -j` command with one exception: how flags are represented.
