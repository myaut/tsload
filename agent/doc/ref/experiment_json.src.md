## experiment.json

experiment.json is a config of TSLoad experiment that is consumed by [tsexperiment][ref/tsexperiment] tool. File have JSON syntax. This document describes it's format.

### Designations

Because JSON have no good human-readable schema, we use our own syntax in this document:

```
<string>			::= '"' text '"'
<number>			::=	"\d+" | "\d+.\d+" | "\d+.\d+e[+-]?\d+"
<value>				::= <string> | <number>

<basic-type> 		::= "node" | "number" | "string" | "boolean" | "node"
<enum-type-value>	::= <string>
<enum-type>			::=	<enum-type-value> "|" <enum-type> | <enum-type-value>
<type>				::= <enum-type> | <basic-type> | "ANY"

<node-description>  ::= "[" <type> "] " <description>
<node-name>			::= <string>

<flag-condition>	::= <node-name> "=" "<value>
<flag>				::= "in" | "out" | "opt" | <flag-condition>
<flag-list>			::= <flag-list> ", " <flag> | <flag>

<node-body> 		::=	<node-description> 
					|   "[" <node-description> ", ..." "]"
					|   "{" <node> ", ..." "}"

<node>				::= "(" <flag-list> ")" <node-name> ":" <node-description>		
<node-list>			::= <node-list> "," <EOL> <node> | <node>

<config-format>		::= "{" <EOL> <node-list> <EOL> "}"		
```

Where flag is one of:
	* __in__ - This node should be provided in config
	* __out__ - This node is written by tsexperiment tool
	* __opt__ - This node is optional and may be ommitted
	* __<node-name> "=" "<value>__ - This node only valid if other node have specified value

### experiment.json

```
{
	(in, out) "name": [string] Name of the experiment,
	(in) "steps" : {
		(in) "Name of workload1" : [node] Steps, ...
	},
	(in) "threadpools" : {
		(in) "Name of threadpool1" : [node] Threadpool, ...
	},
	(in) "workloads" : {
		(in) "Name of workload1" : [node] Workload, ...
	},
	(out) "start_time": [number] Time when workload started in nanoseconds from Unix epoch
	(out) "status": [number] Status code of experiment
	(out) "agent": [node] Information about host and agent that was run this experiment 
}
```

### Threadpool

```
{
	(in) "num_threads" : [number] Number of workers in this threadpool,
	(in) "quantum" : [number] Threadpool quantum in nanoseconds,
	(in) "disp" : {
		(in) "type" : ["round-robin" | "random" | "fill-up" | "user" | 
		               "trace" | "first-free" | "benchmark"] Class of threadpool dispatcher,
		(in, "type" = "fill-up") "n" : [number] Number of requests per batch
		(in, "type" = "fill-up") "wid" : [number] First worker id
	}
	(in, opt) "discard" : [boolean] Discard policy. Default is false
	(in, opt) "sched" : [
		[node] Threadpool scheduler policy, ...
	] 
}
```

#### Threadpool scheduler policy

```
{
	(in) "wid" : [number] Worker id - from 0 to num_threads-1
	(in, opt) "objects" : [
		[string] CPU Object #1, ...
	],
	(in, opt) "policy" : [string] Scheduler policy
	(in, opt) "params" : [node] Scheduler parameters	
}
```

Available scheduler parameters vary from platform to platform.

### Steps 

Constant steps generator:
```
{
	(in) "num_steps" : [number] Number of steps
	(in) "num_requests" : [number] Number of requests per step
}
```

File-based steps generator:
```
{
	(in) "file" : [string] Name of file. It should be located in experiment directory
}
```

### Workloads

```
{
	(in) "wltype" : [string] Type of workload,
	(in, opt) "threadpool" : [string] Name of threadpool which will execute workload's requests, 
	(in, opt) "chain" : [node] Workload chain parameters,
	(in) "rqsched": [node] Request scheduler parameters,
	(in, opt) "deadline" : [node] Deadline of request - maximum time between begin of service and arrival times,
	(in) "params" : [node] {
		(in) "Name of parameter1" : [ANY] Workload parameter 1, ...
	} 
}
```

__NOTE__: You should set "threadpool" or "chain" parameters. 

#### Workload chain parameters

```
{
	(in) "workload"	: [string] Workload name to which current workload is chained
	(in, opt) "probability" : [node] Probability parameters
}
```

Where "probability" is:

```
{
	(in) "value" : [number] Probability of request chaining (from 0.0 to 1.0),
	(in) "randgen" : [node] Random generator
}
```

#### Request scheduler parameters

```
{
	(in) "type" : ["simple" | "iat" | "think"] Type of request scheduler
	
	(in, "type" = "iat", "type" = "think") "distribution" :
				 ["exponential" | "uniform" | "erlang" | "normal" ] Type of random numbers distribution,
	(in, "distribution" = "uniform") "scope" : [number] Value from 0.0 (min = max = mean) to 1.0 (min = 0.0, max = 2 * mean),
	(in, "distribution" = "erlang") "shape" : [number] Shape parameter for Erlang distribution,
	(in, "distribution" = "normal") "covar" : [number] Coeffecient of variation needed to generate stddev value
	
	(in, "type" = "think") "user_randgen" : [node] Random generator that used to generate user ids for request,
	(in, "type" = "think") "nusers" : [number] Number of users to be simulated by think request scheduler
}
```

Since mean value of request arrival time is calculated from number of requests per step (which specified by steps generator), options of iat and think-time request schedulers are differ from random variators options.

#### Random-generated request parameters

For parameters with request scope you may specify random generator options to make them generated randomly for each request:

```
{
	(in) "randgen": [node] Random generator,
	(in, opt) "randvar": [node] Random variator,
	(in, opt) "pmap": [
		[node] Probability map value 1, ...
	]
}
```

"randvar" and "pmap" parameters are mutually exclusive. Only parameters based on integer or floating point may use random variator or pure random generator. For other parameters (such as strings, hostinfo objects, string sets, etc., you should specify probability map that allow to choose one value from discrete set depending of probabilities of this values. Each probability map element defined as:

```
{
	(in) "probability": [number] Probability of this element,
	(in, opt) "value": [ANY] Chosen value
	(in, opt) "valarray": [
		[ANY] value #1, ...
	]
}
```

`value` and `valarray` parameters are mutually exclusive.

### Random generator

```
{
	(in) "class" : ["libc" | "devrandom" | "lcg" | "seq"]	Pseudo-random numbers source,
	(in, opt) "seed" : [number] Generator seed. If not set, then system clock is used
}
```

Best option for random generators is "lcg" or Linear congruential generator. "seq" is a special generator (not random, but uses same interface) that generates values sequentially with step = 1.

### Random variator

```
{
	(in) "class" : ["exponential" | "uniform" | "erlang" | "normal" ] Type of random numbers distribution,
	
	(in, "class" = "exponential", "class" = "erlang") "rate" : [number] Rate parameter,
	(in, "class" = "erlang") "rate" : [number] Shape parameter for Erlang distribution,
	
	(in, "class" = "uniform") "min" : [number] Minimum value for Uniform distribution
	(in, "class" = "uniform") "max" : [number] Maximum value for Uniform distribution
	
	(in, "class" = "normal") "mean" : [number] Mean value for Normal distribution
	(in, "class" = "normal") "stddev" : [number] Standard deviation for Normal distribution
			
}
```

