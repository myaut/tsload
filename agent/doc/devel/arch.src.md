## Architecture

### Agents and TSLoad Core

The key component of TSLoad is an agent, an executable that receives requests from user and sends them to _TSLoad Core_ which is implemented by **libtsload** library. There are several meanings of "receiving requests", but each of them is represented by an appropriate function call. These functions form an _High-level TSLoad API_ which is described in [][tsload/tsload]. 

Most arguments of high-level API function have simple types, but some of them are complex, that is they have multiple variables inside, and some of they may be optional. Complex values are represented by so-called _TSObjects_ (their C type is pointer to tsobj\_node\_t) which true nature depends on agent. Let's take for example function tsload\_schedule\_threadpool which allows to change OS scheduler parameters for threads of a threadpool as described in [Key concepts: threadpools][intro/threadpool]:
```
LIBEXPORT int tsload_schedule_threadpool(const char* tp_name, tsobj_node_t* sched);
```
First argument of that function is name of threadpool. Note that high-level doesn't provide access to an object representing threadpool opposite to a low-level TSLoad functions which are work directly with objects. Second argument is an TSObject. That allows this function being quite flexible. If TSLoad didn't have such parameters, tsload\_schedule\_threadpool had to be separated into several functions:
```
LIBEXPORT int tsload_tp_set_sched_policy(const char* tp_name, int wid, const char* name);
LIBEXPORT int tsload_tp_set_sched_param(const char* tp_name, int wid, const char* param, long value);
LIBEXPORT int tsload_tp_sched_commit(const char* tp_name, int wid);
LIBEXPORT int tsload_tp_bind(const char* tp_name, int wid, cpumask_t*);
``` 
This approach less convenient: for example you may wonder what tsload\_tp\_sched\_commit do. I will only say that if you will write your own agent and forget to call that function, scheduler parameters won't be altered. For a details, see implementation of scheduler utils: [][tscommon/schedutil]. 

Summarizing all of this, _TSLoad_ capabilities is implemented by one of available agents:
* __TSExperiment__ - as of TSLoad 0.2. Standalone agent that consumes request as a an `experiment.json` configuration file which is is JSON format and calls TSLoad High-Level API. Note that TSExperiment don't use TSLoad Core low-level APIs, even if it allows some performance optimizations (i.e. each call with a name of object requires an hash-map search). For TSExperiment, TSObject is nothing by JSON nodes that are parts of `experiment.json` config file. Its sources are located in directory `cmd/tsexperiment`.  
* __TSLoad Daemon__ - partially implemented. This type of agent receives user requests from an _TSLoad server_. Communication protocol is based on JSON, so each request is pre-parsed and TSObjects are passed to a functions calls in "as-is" fashion. Its sources are located in directory `cmd/tsloadd`.
* __TSLoad JNI__ and __TSLoad Python__ - planned. Libraries that allow to call TSLoad APIs from Java or Python, thus TSObjects here arise from VM/Interpreters. 

### Common libraries

TSLoad requires only standard C library, and all needed functionality is implemented in _Common libraries_. I had considered using GLib as an primary library for TSLoad, but it too heavy to be built (and finding and building it for exotic platforms may be hard). Also, TSLoad needs good control of its source code base because each change of it may impose unpredictable experiment result shifts. TSLoad had also used **libjson** to parse incoming JSON, but this library has plenty of problems (i.e. if you miss a comma in experiment configuration file it will say 'Not JSON' and return NULL it a place where NULL shouldn't be returned). libjson was replaced with **libtsjson** which is light-weight, written in C and compatible with TSObject API.

Common libraries are:
* __libtsload__ - as was mentioned earlier, it is _TSLoad Core_ library. 
* __libtscommon__ - library that contains various utilities and facilities, such as memory management, logging, data types, cross-platform filesystem file functions, threads API with scheduler management capabilies. Some of its API functions are inspired by Python, i.e. pathutil.  
* __libtjson__ - light-weight JSON parser & writer.
* __libtsobj__ - TSObject access interface. By default it maps all of its functions to a __libtsjson__ functions, but this behaviour may be overriden.
* __libtsfile__ - library for working with Time-Series File Format (TSF). This format is internally used to keep experiment results by __TSExperiment__ and __TSLoad Daemon__. It is binary file format that has header with schema: offsets of fields, their names and types and array of structures that are written according to schema. 
* __libhostinfo__ - library to get platform information from a computer or a VM where TSLoad agent runs, i.e. number of processors.
* __libtsagent__ - library that implements JSON-TS protocol in C. It is used for communication between __Server__ and __TSLoad Daemon__.

Sources for all libraries are located in `lib` directory.

### Modules

As mentioned in [][intro/intro], TSLoad is modular, thus it dynamically loads external libraries from `INSTALL_MOD_LOAD` directory, and runs their `mod_config` functions. Than that function is called, module registers workload types that it is capable to run, i.e. _http_ module may run web workloads. Modules may be written outside of TSLoad as described in [][intro/module]. Modules that are shipped with TSLoad are located in `mod` subdirectory.

### Server

__TSLoad Server__ allows to manage multiple instances of agents. Technically server is only a router that receives JSON-TS messages from multiple endpoints, check ACLs for that endpoints and forwards them to a desired agent. Server is written in Python and works on top of [Twisted framework](https://twistedmatrix.com/). 

