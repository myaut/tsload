
## Key concepts: Workloads

To understand term _workload_, first we need to deal with term _request_. Good explanation is given by authors of Magpie tool:

> A __request__ is system-wide activity that takes place in response to any external stimulus of the application(s) being traced. For example, the stimulus of an HTTP request may trigger the opening of a file locally or the execution of multiple database queries on one or more remote machines, all of which should be accounted to the HTTP request. [1](#1)

However, such definition is correct for OS or application point of view. In the TSLoad we could define _request_ quite different: "A __request__ is a set of parameters that cause desired system-wide activities and another set of characteristics that describe these activities". Thus __workload__ is a batch of related requests:

> The requests made by users of system are called workloads. For example, CPU workload would consist of the instructions it is asked to execute. The workload of a database system would consist of queries and other requests it executes for users. [2](#2)

#### Defining workloads

To define workload, we have to characterize it and pick proper module that implements such workload. First of all you need to determine which computing resource you want load: i.e. network or level 1 cache of processor. Then you have to select __workload type__ that generates correct workload for this resource. All workload types in TSLoad are classified by resource they loading (this is represented by bitmask [wl_class][tsload/wltype#typedef_enum_wl_class] values).  Lets run tsexperiment with subcommand `workload` to determine which workload types are available:
```
$ ./bin/tsexperiment workload
WLTYPE           CLASS              MODULE      
http             --------------Nc-- http        
bigmem           C--m-------------- bigmem      
simpleio_write   ------F-rwDrw----- simpleio    
simpleio_read    ------F-rwDrw----- simpleio    
busy_wait        C----------------- busy_wait 

(some output is omitted)
```

As you can see from this output, there are two CPU-intensive workload types - bigmem and busy_wait, two disk I/O intensive workloads (the are labeled by large 'D' letters that stands for 'Disk' and large 'F' which stands for 'Filesystem') from simpleio module, and one network client workload type - 'http'. These letters are described in [tsexperiment][ref/tsexperiment#workload] page.

When you have chosen workload type, it's time to attach it to __threadpool__. It could be done directly (by simply specifying its name as 'threadpool' parameter) or indirectly - through 'chain' parameter. In second case TSLoad will chain workloads requests and execute them sequentially without any delay. _Chaining workloads_ is useful when you want to run batch of CPU instructions followed by some I/O and do not want to write new module for it. You could also set chaining 'probability' if not all requests have subsidiaries.

__NOTE__: Due to logic of chaining mechanism, you should multiply probabilities. For example if you have chain consisting from workloads abc, pqr and xyz, and you set probabilities to 0.5, than actual probability of workload 'xyz' would be 0.25 beause if pqr request wouldn't be chained to abc, then TSLoad wouldn't try to do the same with xyz request.

Third main characteristic of workload is time series of number of requests per step, called 'steps'. They are provided by __step generator__ which run on server-side or inside tsexperiment tool. Latter supports two step generators: _constant_, which would generate stationary workload with equal number of requests on each steps and _file_ which allows to set number of requests on per-step basis.

#### Request scheduling

Schedulers are responsible for generating request arrival times. There are three request schedulers available in TSLoad:
 * __simple__. This scheduler sets arrival time for all requests to zero, so it forces threadpool to execute request ASAP. It is reasonable option for benchmark runs and also may be of use with _fill-up_ threadpool dispatcher cause it allows to create huge batch of requests (but run them from beginning of step however).
 * __iat__. This scheduler generates inter-arrival time according to selected distribution law and commonly used, because it allows to create M/M/n experiments.
 * __think__. This is more complicated version of _iat_ scheduler that intended to be more realistic. While _iat_ assumes that all requests are independent, actually when user works with dialog system (like website), time of next arrival (click on hyperlink) depends on service time for previous page. In this case time interval between loading page (end of service #n) and click on hyperlink (arrival time #n+1) should be distributed randomly. So, _think_ dispatcher assigns request two one of N users (while N is configurable variable) and distributes interarrival times according to selected distribution law. But when user X finishes it's request, it walks over requests queue and adds service time two scheduled arrival time of each request of that user. 
   However, this scheduler induces more performance effects, and we recommend to use __iat__ scheduler where possible. It is also useful along with _user_ threadpool dispatcher that was described earlier. 

Also there is common parameter called __deadline__ that is useful in simulating real-time processes. If request start it's execution after (arrival time + deadline), then TSLoad discards such request. Default value for that parameter is 292 years.

#### Parameters

Request _parameters_ are passed from JSON-based config to workload module to describe how it should behave to run request. Like variable they could have different scopes. 
	* _Workload_ parameters that do not change during lifetime of workload - so they ususally affect workload preliminary configuration (for example, for filesystem I/O simulation if space for files is preallocated file size would be workload parameter).
	* _Request_ parameters are opposite: they could be set individually per each request. For example, it could be block number for disk i/o benchmark or length of network packet. 
		* _Output_ parameter is a kind of request parameter that is not configurable and filled in by module. For example, for HTTP workload it could be HTTP status code for request
	Some parameters are optional and have default value, so they could be omitted from config.

	Each parameter have type which says TSLoad how to process it. Type is identified by [wlp_type_t][tsload/wlparam#wlp_type_t] enumeration value:
 
---
__Base type__ | __Type__ | __JSON node in config__ | __C variable type__ | __Description__
2,1 WLP_BOOL | Boolean | wlp_bool_t | Boolean type
1,3 WLP_INTEGER | WLP_INTEGER | Number | wlp_integer_t | Abstract 64-bit signed integer
 WLP_SIZE | Number | wlp_integer_t | Size in bytes 
 WLP_TIME | Number | wlp_integer_t | Time interval
2,1 WLP_FLOAT | Number | wlp_float_t | Floating point number with double precision
1,2 WLP_RAW_STRING | WLP_RAW_STRING | String | char[] | String
 WLP_FILE_PATH | String | char[] | Path to file
2,1 WLP_STRING_SET | String | wlp_strset_t (int) | Special type to create enumerations
1,2 WLP_HI_OBJECT | WLP_CPU_OBJECT | String | wlp_hiobject_t | hostinfo CPU object
  WLP_DISK | String | wlp_hiobject_t | hostinfo disk object
---

However, some of these types are just hints, for example WLP_TIME is alias for WLP_INTEGER. Let's again run tsexperiment tool and take closer look at parameter view of bigmem workload.

```
$ ./bin/tsexperiment workload
WLTYPE           CLASS              MODULE      
        PARAM            SCOPE OPT  TYPE   RANGE      DEFAULT
bigmem           C--m-------------- bigmem      
        mempool_size     WL        SIZE                    
                Memory pool size
        cycles           RQ        INT                     
                Number of cycles
        offset           RQ        INT                     
                Starting offset inside memory pool
        step             RQ        INT                     
                Iteration step over memory pool
        access           RQ        STRSET mm,mr,rr         
                Indicates if operation is memory-intensive or not 
        instruction      RQ        STRSET sum,mul,cmp       
                Instruction to be executed (sum, mul, cmp)
```

As you can see, there are on workload-scoped parameter called `mempool_size` which is integer - but have 'SIZE' hint. It  Other four types are request scoped. `access` and `instruction` parameters are enumerations. Let's create parameters vector for that workload:

```
		...
		"params": {
			"mempool_size": 268435456,
			"cycles": 100000,
			"offset": 0,
			"step": 64,
			"access": "mm",
			"instruction": "mul"
		}
		...
``` 

Good, but not enough. First of all, despite `access` variable says that transfers should be memory-memory, they won't be. Thats because with step = 64, offset = 0, and cycles = 100000, so all requests will walk first 6.4 megabytes of memory pool, so they basically will do cache-cache transfers, because modern L3 caches are enough to store such amounts of memory. Also, have you seen workloads that only multiply? We just created one. So we need to make per-request parameters random.

TSLoad have three possibilities to create random-generated request parameters:
	* __Random generator__. Because they are uniformally distributed, they are good for generating integers which distribution is irrelevant - in our case it is `offset` variable. But do not forget to modulo offset by mempool_size if you write such module or you will get "Segmentation fault"
	* __Random variator__. They use random generators and variate their number according to selected distribution. Obvious candidate to do so is `cycles` parameter which is related to duration of executing requests. By creating exponential variator for cycles parameter we will make our workload closer to M/M/n queue. 
	* __Probability map__. It also uses random generator, but to generate discrete values with different probabilities. So we could modify `instruction` parameter to create mix of instructions. 
	
So our new configuration would look like:

```
		...
		"params": {
			"mempool_size": 268435456,
			"cycles":  {
				"randgen" : { "class" : "lcg" },
				"randvar" : {
					"class": "exponential",
					"rate": 1e-05
				}
			},
			"offset": {
				"randgen" : { "class" : "lcg" }
			},
			"step": 64,
			"access": "mm",
			"instruction": {
				"randgen" : { "class" : "lcg" },
				"pmap": [
					{
                       "value": "cmp", 
                       "probability": 0.2
                    }, 
                    {
                       "value": "sum", 
                       "probability": 0.2
                    }, 
                    {
                       "value": "mul", 
                       "probability": 0.6
                    }
                ]
			}
		}
		...
``` 

LCG is a abbreviation for [Linear congruential generator](http://en.wikipedia.org/wiki/Linear_congruential_generator).

##### References

1. [1] Paul Barham, Austin Donnelly, Rebecca Isaacs, and Richard Mortier. 2004. Using magpie for request extraction and workload modelling. In _Proceedings of the 6th conference on Symposium on Opearting Systems Design & Implementation - Volume 6 (OSDI'04)_, Vol. 6. USENIX Association, Berkeley, CA, USA, p. 260.
2. [2] The Art of Computer Systems Performance Analysis - by Raj Jain (ISBN 0471-50336-3, 1991, 685 pages, John Wiley & Sons Inc., New York), p. 4.