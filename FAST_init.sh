#!/bin/bash
#hashpipe -p ./FAST_hashpipe -I 0 -o BINDHOST="10.10.12.2" -c 0 FAST_net_thread -c 2 FAST_gpu_thread -c 4 FAST_output_thread                  # for UCB-RAL-snb11

#hashpipe -p ./FAST_hashpipe -I 0 -o BINDHOST="192.168.1.127" -c 18 FAST_net_thread -c 20 FAST_gpu_thread -c 22 FAST_output_thread		# for China-NAO-m21
#hashpipe -p ./FAST_hashpipe -I 0 -o BINDHOST="192.168.16.11" -c 18 FAST_net_thread -c 20 FAST_gpu_thread -c 22 FAST_output_thread		# for China-NAO-m21
hashpipe -p ./FAST_hashpipe -I 1 -o BINDHOST="0.0.0.0" -c 18 FAST_net_thread -c 20 FAST_gpu_thread -c 22 FAST_output_thread		# for China-NAO-m21

