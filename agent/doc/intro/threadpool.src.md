
## Key concepts: Threadpools

Since TSLoad is designed to simulate multi-user environments, it has concept of a _worker_, elementary process that handles user requests. For real application meaning of "worker" may vary - it may be OS-level process or thread or userspace thread. Latter is usually referred as _asynchronicity_, because such frameworks usually spawn one userspace thread per user request, so all each request handled asynchronously. Examples of userspace threads are Erlang processes, Scala actors or Python Twisted deferreds (they actually use Python generators). Currently only worker supported by TSLoad is OS-level thread, so we will refer worker pool as __threadpool__ in this documentation (and code too).

Threadpool consists of control thread and set of worker threads. Threadpool is synchronous: timeline of threadpool is separated into steps, and at beginning of each step, it generates requests that have to be executed during this step. Step duration interval is constant and called _quantum_, while second primary parameter of threadpool is _num\_threads_ that represents number of workers in this pool. Each step is parametrised by number of requests per each workload class that should be run during this step.

__NOTE__: Threadpools are deprecated as of TSLoad 0.2. They will be replaced with more flexible and modular __loaders and channels__.

#### Threadpool dispatchers
 
At beginning of each step requests are represent a queue, so they have to be dispatched to worker threads. To do so, worker and control threads are calling dispatcher. There are three main implementations of it in TSLoad 0.2:
    * __Queue__ dispatchers. This dispatcher pre-distributes requests across workers at beginning of each step. This may induce some unwanted effects: for example if inter-arrival time between requests 3 and 4 is 10 us (and mean service time is 10ms) and due to random policy these requests were dispatched to same worker. In this case, you will get unwanted wait time for request 4. Such dispatchers are good when users set is constant (i.e. intranet system like SAP), while they generate requests more than once. This dispatcher is represented by set of policies:
        * __Round-robin__ - during dispatching workers are switched in circular order  
        * __Random__ - each worker is selected randomly
        * __Fill-up__ - requests are dispatched by batches. For example, for fill-up dispatcher with n=4 and wid=2, it dispatch requests 1-4 to worker #2, 5-8 to worker #3, and so on. Along with "simple" request scheduler this dispatcher is useful when you need to create where intensive batch of requests for a short time (spike).
        * __User__ - requests are dispached according to its `user` field. Note that this field is filled only by think-time request scheduler
        * __Trace__ - requests are dispatched according to field `thread`. This is useful for trace-driven simulations
    * __First-free__ dispatcher. This kind of dispatcher doesn't do pre-distribution of requests and dispatches request only when it's arrival time comes. Because of that, it is more complicated than queue-based dispatcher and causes more performance effects. Dispatching is handled by control thread that picks random worker, but if it is busy, it walks list of workers and selects first free worker. If all workers are busy at the moment of arrival, it sleeps (waits on conditional variable) until step ends or some worker will finish executing of current request picks latest request and wake ups control thread so it could dispatch next request. Unlike queue-based dispatcher this mode is intended to simulate flows of independent requests.
    * __Benchmark__ dispatcher. While not being true dispatcher, it uses same API, so we will describe it here. Unlike the other ones, it doesn't respect number of requests per step, and executes queue of requests in circular way until step ends. By doing this it measures maximum number of requests that system can handle; thus it measures system _throughput_. 

There also one parameter common for all dispatchers - _discard_. If it is set to true, then dispatcher will discard all requests that remain from previous step.
	
#### Threadpool scheduling

TSLoad also capable of setting OS scheduler options for worker threads. This functionality provided by libtscommon "schedutil" module and works on all platforms (while scheduler options may vary on different platforms). Workers may be bound to the specific CPU object, which could be chip, NUMA node, core or hardware thread (or strand in terminology of TSLoad) or cache object. You may also create set of them. Actual binding through "set thread affinity call" (for example, sched_setaffinity() in Linux) and selected set of objects is converted to CPU mask. Second scheduling capability is setting parameters (that are usually policy and priority) individually for each thread.  

__NOTE__: Solaris doesn't have such "affinity call" - instead of that it provides processor_bind() that could bind thread only to single hardware thread. If you specify more than one thread in _objects_ parameter, TSLoad will attempt to create a processor set which uses processors exclusively. 

To list available scheduler options and CPU objects, use [tshostinfo][ref/tshostinfo] command.

#### Configuration examples

Here are simplest thread pool that constists of 2 threads and with one-second quantum (all times in TSLoad have nanosecond-resolution):

```
"test_tp" : {
   "num_threads": 2,
   "quantum": 1000000000,
   "disp": {
    	"type": "random"
   }
}
```

Here an example of "sched" parameter used to set scheduler options for Linux:
```
	"sched": [
    	{ "wid": 0,
    	  "objects": ["strand:0:0:0"],
    	  "policy": "idle",
    	  "params": { "nice" : 0 } },
    	{ "wid": 1,
    	  "objects": ["strand:0:0:0"] },
    	{ "wid": 2,
    	  "objects": ["strand:0:0:0"],
    	  "policy": "normal",
    	  "params": { "nice" : 5 } },
    	{ "wid": 3,
    	  "objects": ["strand:0:0:0"] }            	 
    ]
```

All of four workers are bound to same hardware thread, policy of first worker (which is identified by "wid" and enumerated from zero) is set to idle, and priority of third worker is adjusted. 