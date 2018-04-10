#!/bin/bash
hashpipe -p ./FAST_hashpipe -I 0 -o BINDHOST="10.10.12.2" -c 0 FAST_net_thread -c 2 FAST_gpu_thread -c 4 FAST_output_thread 
#hashpipe -p ./FAST_hashpipe -I 1 -o BINDHOST="192.168.1.127" -c 1 FAST_net_thread -c 2 FAST_gpu_thread -c 3 FAST_output_thread
#&
#hashpipe -p FAST_hashpipe -I 1 -o BINDHOST="10.10.13.3" -c 4 FAST_net_thread -c 5 FAST_gpu_thread -c 6 FAST_output_thread

#hashpipe -p ./FAST_hashpipe -I 2 -o BINDHOST="192.168.1.127" -c 18 FAST_net_thread -c 20 FAST_gpu_thread -c 22 FAST_output_thread

