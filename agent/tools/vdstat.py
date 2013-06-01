#!/usr/bin/env python

import sys
import csv

import math

RQF_FINISHED = 8

if len(sys.argv) != 2:
    print 'Usage: vdstat.py <requests-file>'
    sys.exit(1)
    
rq_file = csv.reader(file(sys.argv[1]))

cur_step = 0

print  " interval  i/o   MB/sec   bytes   read     resp     resp     resp "
print  "          rate  1024**2     i/o    pct     time      max   stddev "

io_total, read_count, write_count = 0, 0, 0
resp_times = []

def report():
    io_count = read_count + write_count
    read_pct = 100 * float(read_count) / float(io_count)
    
    r_max = 0
    r_mean = 0
    r_stddev = 0
    
    r_max = float(max(resp_times))
    r_mean = float(sum(resp_times)) / len(resp_times)
    
    for r_time in resp_times:
        r_stddev += float(r_mean - r_time) ** 2
    r_stddev = math.sqrt(r_stddev / len(resp_times))
    
    print '%8d %8d %8.4f %8d %8.2f %8.3f %8.3f %8.3f' % (cur_step,
                                                     io_count,
                                                     io_total / 1024. / 1024.,
                                                     io_total / io_count,
                                                     read_pct,
                                                     r_mean / 1000. / 1000.,
                                                     r_max / 1000. / 1000.,
                                                     r_stddev / 1000. / 1000.)

for row in rq_file:
    wl_name, step, rq_id, tid, start, end, rq_flags = row
    wl_type, xfersize, dev = wl_name.split('-') 
    
    rq_flags = int(rq_flags, 16)
    
    if not (rq_flags & RQF_FINISHED):
        continue
    
    step = int(step)
    start = int(start)
    end = int(end)
    xfersize = int(xfersize)
    
    resp_times.append(end - start)
    io_total += xfersize
    
    if wl_type == 'read':
        read_count += 1
    elif wl_type == 'write':
        write_count += 1
    
    if step != cur_step:
        report()
        
        io_total, read_count, write_count = 0, 0, 0
        resp_times = []
        
        cur_step = step
    
    