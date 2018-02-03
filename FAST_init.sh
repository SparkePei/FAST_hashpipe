#!/bin/bash
hashpipe -p ./FAST_hashpipe -I 6 -o BINDHOST="10.10.12.2" -c 1 FAST_net_thread -c 2 FAST_gpu_thread -c 3 FAST_output_thread 

#&
#hashpipe -p FAST_hashpipe -I 1 -o BINDHOST="10.10.13.3" -c 4 FAST_net_thread -c 5 FAST_gpu_thread -c 6 FAST_output_thread


