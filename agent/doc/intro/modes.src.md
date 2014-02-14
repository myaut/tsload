
## Operating modes of TSLoad

As we mentioned in [Introduction][intro/intro], TSLoad is universal loader because it supports multiple operating modes. 

#### Microbenchmarking mode

Microbenchmarking mode is a simplest and usually indended to measure system's throughput (which usually can be described by 'operations per second') by running as much as possible identical requests:
	* Random generated per-request parameters are not recommended. However for disk I/O benchmark block number still could be random.
	* Request scheduler: 'simple'
	* Threadpool dispatcher 'benchmark'
	* Use constant step generators (however, number of requests per step parameters is irrelevant and could be set to 1). 

#### Simulation mode

In simulation mode TSLoad tries to reproduce real-world workload using some characteristics that was previously measured on target system. However, these characteristics may be cumulative so it is up to you to pick correct experiment parameters. Simplest example of simulation mode is G/G/n queue simulation:  
	* Request scheduler is 'iat'. 
	* Select primary workload parameter that affects service time - such as number of cycles or block size and user random variator for it.
	* Set threadpool dispatcher to 'first-free'. For G/G/1 runs you may use one of queue-based dispatchers because dispatching policy is irrelevant in that case.
	* For CPU-bound workloads, create threadpool with n workers. I/O bound workloads support only G/G/1 mode.
	* Constant step generator is recommended for simulation mode

If you set request scheduler and primary workload parameter distribution parameter to 'exponential' then you will get nearly M/M/n queue, however, it is not possible to control service time because it depends on nature of resource too.

#### Time-Series mode

Time series mode is identical to simulation mode with non-stationary simulations - in this case you should use file-based step generator. For example you may want to test some cloud platform that states that it supports elastic computing. So you can create two workloads that work in different phases: one generating intensive load for first 600 steps and the other for next 300 and see how hypervisor trying to adapt.

#### Trace-driven mode

In trace driven mode you can use trace that you collected from real system. Simplest example of trace driven load run is trace-reproduction mode of tsexperiment tool that allows to reproduce previous experiment which is enabled by specifying -T option for run subcommand. 
	* Request scheduler is irrelevant cause it is disabled.
	* Threadpool dispatcher should be 'trace' (tsexperiment do this automatically) 
	* Per-request parameters are irrelevant because they are read from trace file  
	* Data provided by step generators should match number of requests in trace file 

