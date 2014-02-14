
## Running experiments with tsexperiment

Now it's time to run some workloads. As of TSLoad 0.2 it's the only way to run experiment because it doesn't support multi-agent mode, so first of all you will need to create a directory containing [experiment.json][ref/experiment_json] file with experiment's configuration which defines [threadpools][intro/threadpool] and [workloads][intro/workload]. We omit this step in this guide because it was discussed earlier. 



If you do so - let's check that config is parseable by TSLoad. I will use sample experiment shipped with TSLoad:

```
$ ./bin/tsexperiment -e var/tsload/sample/ show
name         : test
root         : var/tsload/sample/
directory    : 
runid        : -1
status       : NOTCFG

THREADPOOL       NTHRS  QUANTUM  DISP        
test_tp          2      1s       random      

WORKLOAD         TYPE         THREADPOOL       RQSCHED     
test1            busy_wait    test_tp          think       
test2            busy_wait    test_tp          iat         
test3            busy_wait    -                -            chained: test2

```

As you can see there are one threadpool called test_tp and three workloads attached to it.

```
$ ./bin/tsexperiment -e var/tsload/sample/ run
Created const steps generator for workload 'test1' with N=20 R=2000
Loaded steps file 'var/tsload/sample/test2.tss' for workload 'test2'

=== CONFIGURING EXPERIMENT 'test' RUN #3 === 
Configured threadpool 'test_tp' with 2 threads and 1s  quantum
Workload 'test1' configuring (0): 
Workload 'test1' successfully configured (100): 
Workload 'test2' configuring (0): 
Workload 'test2' successfully configured (100): 
Workload 'test3' configuring (0): 
Workload 'test3' successfully configured (100): 

=== STARTING WORKLOADS @01:43:22 14.02.2014 === 
Starting workload 'test1': provided 15 steps
Starting workload 'test2': provided 15 steps
Starting workload 'test3': provided 0 steps
...
(some output is ommitted)
...

=== UNCONFIGURING EXPERIMENT 'test' RUN #3 === 
...
(some output is ommitted)
...

```

Experiment consists of several runs which could be listed using list subcommand:

```
$ ./bin/tsexperiment -e var/tsload/sample/ list
ID   NAME             STATUS DATE                 HOSTNAME
...
3    test             FINISHED 01:43:22 14.02.2014  panther.mylocal
```

Timestamp in column `DATE` is a scheduled start time of experiment's workloads, so if you use some external monitoring tool, use it as baseline to correlate values. 

Let's take closer look to subdirectory created by tsexperiment for test run number 3:

```
$ ls -l var/tsload/sample/test-3/
total 1662                                                                                                                                                                                    
-rw-r--r-- 1 myaut users    1715 Feb 13 21:41 experiment.json 
-rw-r--r-- 1 myaut users    1411 Feb 13 21:40 experiment.json.old   
-rw-r--r-- 1 myaut users     870 Feb 13 21:40 test1-schema.json 
-rw-r----- 1 myaut users 2564096 Feb 13 21:41 test1.tsf 
-rw-r--r-- 1 myaut users     870 Feb 13 21:40 test2-schema.json 
-rw-r----- 1 myaut users   22528 Feb 13 21:41 test2.tsf
-rw-r--r-- 1 myaut users      51 Feb 13 21:41 test2.tss 
-rw-r--r-- 1 myaut users     870 Feb 13 21:40 test3-schema.json
-rw-r----- 1 myaut users    9728 Feb 13 21:41 test3.tsf 
-rw-r--r-- 1 myaut users    3402 Feb 13 21:41 tsexperiment.out
```

As you can see there is copy of `experiment.json` and `test2.tss` configuration files, file called `tsexperiment.out` that contains output of tsexperiment utility and trace files with tsf exception (which means TS File). Because it is binary-formatted there is a schema file nearby that helps TSLoad to parse that file. 

You may alter some of experiment parameters on the fly. For example let's increase number of requests per step for workload 'test1' to 8000 and 12000. Let's dump experiment config in name-value format:

```
$ ./bin/tsexperiment -e var/tsload/sample/ show -l
name=test
steps:test1:num_steps=20
steps:test1:num_requests=2000
...
(some output is ommitted)
```

Parameter called 'steps:test1:num_requests' is what we need. It would be also useful to specify `-n` option to create custom run name. Now rerun experiment with these values:

```
$ ./bin/tsexperiment -e var/tsload/sample/ run -s steps:test1:num_requests=8000 -n run-8k
...
(some output is ommitted)
```

If you run command like `top` or `mpstat` you will see that processor consumption for tsexperiment is increased. You will also may want to reproduce experiment precisely - in this case you need to run tsexperiment with -T option:

```
$ ./bin/tsexperiment -e var/tsload/sample/ run -T 3 -n trace
...
(some output is ommitted)
```

So when we will rerun 'list' subcommand, we will se new experiment runs:

```
$ ./bin/tsexperiment -e var/tsload/sample/ list
ID   NAME             STATUS DATE                 HOSTNAME
...
11   run-8k           FINISHED 02:37:11 14.02.2014  panther.mylocal
12   run-12k          FINISHED 02:37:59 14.02.2014  panther.mylocal
13   test             FINISHED 02:39:15 14.02.2014  panther.mylocal
```

Write these numbers down because we will need them onto next step.