## tsexperiment

##### Name
TS Experiment - standalone agent to run TSLoad experiments 

##### Synopsis
`$ tsexperiment [global options] subcommand [options and arguments]`

##### Description
Where global options are:
	* -e <experiment_path>
		Path to experiment root directory
	* -d|-t 
		Enable debug or tracing

Experiment constsists of current config and several "runs", that contain versions of config (maybe with altered parameters) and experiment results. Experiment config is a file named [experiment.json][ref/experiment_json] Each run identified by it's unique number called __runid__. 

### List capabilities

[workload]

`$ tsexperiment info [CAPABILITY] [-x] [-H] [-l] [-j] [NAME]...`
Lists supported TSLoad Core capabilities with it's parameters. If NAME arguments are specified, shows only information on desired capabilities.

Options:
	* -j - Print in JSON format   
	* -x - Print extended data such as parameters and descriptions
	* -H - Do not print headers
	* -l - Print legend

#### Output description for workload types

Output columns for workload types:
   * __WLTYPE__ - name of workload type
   * __MODULE__ - name of the module that provides it
   * __CLASS__ - bitmask of workload class
       * _C_ - CPU intensive workloads. May be followed by _i_ or _f_ flags: integer or floating-point operations respectively and _m_ flag for memory-intensive workloads.
       * _M_ - virtual memory workloads. Only available subflag is _a_ for memory allocation
       * _F_ - filesystem workloads. Subflag _o_ stands for operations such as making directories, creating/deleting files, etc. while _rw_ subflag means read-write workload.
       * _D_ - disk workload. Only available subflag is _rw_ for read-write
       * _N_ - network workload. May be followed by _c_ meaning that actual workload is for some network application
       * _O_ - miscellanous OS-level workload

Output columns for workload params:
    * __PARAM__ - name of parameter
    * __SCOPE__ - scope of parameter (_WL_ for workload, _RQ_ for per-request parameters and _OUT_ for output parameters)
    * __OPT__ - flag that parameter is optional
    * __TYPE__ - type of parameter
    * __RANGE__ - minimum and maximum values for integer and float types, maximum length of string or available values for string sets
    * __DEFAULT__ - default parameter value (if provided by module)
        
### Working with experiments

#### Show information about experiment

`tsexperiment -e <experiment_path> list`
Lists experiment runs

Output columns:
	* __ID__ - id of experiment or so-called __runid__
	* __NAME__ - name of experiment run
	* __STATUS__ - status of experiment run. _FINISHED_ - run was successfully finished, __ERROR__ - error was encountered during experiment run, __NOTCFG__ - experiment was not configured
	* __DATE__ - date and time when workload start was scheduled
	* __HOSTNAME__ - name of host where experiment was run


`tsexperiment -e <experiment_path> show [-j] [-l] [RUNID]`
Shows experiment run information and config: enlists global options such as runtime, information of host where experiment was run, workloads and threadpools.

Options:
	* -j - Show in JSON format
	* -l - Show as name-value list (useful to create run -s option arguments)

#### Running experiments

`tsexperiment -e <experiment_path> run [-n name] [-s param=value] [-b] [-T] [RUNID]`
Creates new experiment run and starts it

Options and arguments: 
	* -n - sets name of experiment 
	* -s - sets variable of experiment config only for current run (see below)
	* -b - enables batch mode (see below)
	* -T - enables trace-driven mode. In this mode tsexperiment uses traces generated during previous run (specified as runid). It also changes dispatchers of all threadpools to trace mode. Request arrival time, per-request parameters are used, and also requests are dispatched to same workers it was executed during baseline run.

##### Running experiments in batch mode

Batch mode is intended to run set of experiments from shell-script and enabled by setting `-b` option. In this mode `tsexperiment` doesn't print information about course of experiment (it is still saved in <run_directory>/tsexperiment.out file) and only prints runid to stdout so you can use it in your script. For example you may write bash script:

```
#!/bin/bash
EXPERIMENT=/var/tsload/sample/
RUNID=$(tsexperiment -e $EXPERIMENT run -b)
tsexperiment -e $EXPERIMENT show $RUNID
```

Also experiment config variables may be altered by using -s option, where param name is path to JSON node inside config. I.e:
```
run -s threadpools:tp_test:num_threads=10
run -s workloads:workload1:params:cycles:pmap:0:probability = 0.3
```
	
#### Working with experiment results

`tsexperiment -e <experiment_path> report RUNID [WL]...`  
Show some statistics report about experiment run. If WL arguments are specified than tsexperiment reports only particular workloads. RUNID argument is mandatory.

Output columns:
	* __STEP__ - id of step
	* Request numbers
		* __COUNT__ - requests that were scheduled to be executed during this step
		* __ONTIME__ - requests that were started their execution according to its arrival time
		* __ONSTEP__ - requests that were finished before it's step ended
		* __DISCARD__ - requests that were discarded due to threadpool policy or request scheduler deadline parameter
	* Times in milliseconds - mean and standard deviation
		* __WAIT TIME__ - time between request arrival and start of service
		* __SERVICE TIME__ - time that was spent by worker while executing request

`tsexperiment -e <experiment_path> export [-d DEST] [-F csv|json|jsonraw] [-o option] [-o wl_name:option] RUNID [WL]...   `
Export workload measurement data into text files

Options: 
	* -d - destination directory of output. If not set, then experiment run directory is used
	* -F - desired format of output.
	* -o option - options for TSFile backend. See more at [tsfutil][ref/tsfutil]

### Exit status

If command was successfully executed, `tsexperiment` returns 0 as exit status. If invalid command line was passed to `tsexperiment` it returns one of the values from range {1..5}. In case of other failures, it will return 30 (`CMD_GENERIC_ERROR` code), or one of the _well known_ error codes from range {11..14}.

---
Code | Mnemonic | Description
 |2,1 __Basic errors__
0 | `CMD_OK` | Everything went fine
1 | `CMD_MISSING_CMD` | Subcommand is missing from command line
2 | `CMD_UNKNOWN_CMD` | Unknown subcommand
3 | `CMD_INVALID_OPT` | Incorrect option was specified or option needs argument
4 | `CMD_INVALID_ARG` | Invalid option or argument value (like negative integer)
5 | `CMD_MISSING_ARG` | Required option or argument is missing 
 |2,1 __Experiment errors__
11 | `CMD_NOT_EXISTS` | Needed file not exists 
12 | `CMD_NO_PERMS` | Insufficient permissions to read or write on of the files
13 | `CMD_ALREADY_EXISTS` | TSExperiment tried to create file, but it is already exists
14 | `CMD_IS_ROOT` | Some operation that TSExperiment had tried to perform on root experiment only allowed for experiment runs
30 | `CMD_GENERIC_ERROR` | All other errors
---

Experiment errors will be accompanied with `EXPERR` error code printed to standard error output. 