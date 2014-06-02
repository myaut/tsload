### TSLoad 0.3

TSLoad server release + monitoring functionality embedded into agent
Release date: ?

### TSLoad 0.2

Final release of 0.2 branch
Release date: ?

* Tuneables API
* Workload advanced parametrization: probability arrays with O(1) complexity
* HTTP request module

### TSLoad 0.2 alpha

Iterative release that incorporates a large subset of functionality.
Release date: 15 Feb 2014.

* SCons build system for agent
* Support for cross-platform builds
* TSExperiment - standalone runner without server
* Enhanced threadpools features: scheduler control, dispatchers, benchmark mode
* Random generators and variators
* Workload advanced parametrization: randomly generated parameters, output, per-request parameters, meta-types
* Workload chaining
* Workload types - more than one workload per module
* Request schedulers
* Integration with OS tracing facilities
* TSFile - output format for experiments
* Documentation
* Unit testing for libraries

### TSLoad 0.1

Proof-of-Concept internal release. 
Release date: 10 Dec 2012.

* TSCommon library basic features (logging, threads, etc.)
* Agent daemon tsloadd for running experiments
* Basic support for modules 
* Simple workload parametrization
* JSON-TS protocol for agent-server interaction 
* Support for threadpools - a set of workers that process requests
* TSLoad Server written on Scala
* simpleio and busy_wait modules