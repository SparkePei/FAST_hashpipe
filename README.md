# FAST_Hashpipe
### Introduction
    
* This  code is used to receive the packets from high speed ethernet, calculate the Stokes parameters and save the data to disc in filterbank format.The data will be stored in a temporary place which mounted on CPU RAM. A FRB real time search software known as Heimdall will look for the new *.fil file in a given directory, once Heimdall found interested signals the Filterbank files will be moved to disk, otherwise remove these raw data.<br>
* Three threads have been developed to perform packet receiving, Stokes calculation and Filterbank data formatting, which are:
    * FAST_net_thread; (packet receving)
    * FAST_gpu_thread; (Stokes calculation,collapse data in time and frequency,"gpu" is just a tradition name, no GPU used in here.)
    * FAST_output_thread; (Filterbank data saving)

* There are 2 buffers between three threads, each of them has a 3 segments ring buffer.  Buffer status could be abstracted from each ring buffer. There is a demo about how does Hashpipe working you can find [in here](https://github.com/SparkePei/demo1_hashpipe).

### Installation
* Required packages as follows:
    ```
    Hashpipe 1.5
    Ruby 2.1.10
    rb-hashpipe 1.5
    ```
    [Here](https://github.com/SparkePei/demo1_hashpipe) is the tutorial to install these packages.
* once these required packages installed properly, you can download this software from github:
    ```
    git clone https://github.com/SparkePei/FAST_hashpipe.git
    ```
* enter this directory and run:
    ```
    make (do "make clean" before you recompile)
    sudo make install
    ```
### How to run this software
* To (re)start FAST_hashpipe on all compute nodes:
    ```
    log onto frbseti as user obs: ssh -X obs@localhost
    fastburst_restart_all.sh
    ```
* To stop FAST_hashpipe on all compute nodes:
    ```
    log onto frbseti as user obs
    fastburst_stop_all.sh
    ```
* To (re)start FAST_hashpipe on single compute nodes:
    ```
    log onto compute node as user obs: ssh obs@mxx
    fastburst_restart.sh
    ```
* To stop FAST_hashpipe on single compute nodes:
    ```
    log onto compute node as user obs
    fastburst_stop.sh
    ```
* You can easily tap following command to start at your installation directory:
    hashpipe -p FAST_hashpipe -I 0 -o FIL_LEN=80 -c 18 FAST_net_thread -c 20 FAST_gpu_thread -c 22 FAST_output_thread < /dev/null  1> fastburst_${host_name}.out.$log_timestamp 2> fastburst_${host_name}.err.$log_timestamp &
    In here, "FAST_hashpipe" as plugin was launched by hashpipe software and created an instance of "-I 0". "-c 18 FAST_net_thread" is used to assign the CPU 18 for FAST_net_thread, and so on. You can set filterbank file length to 80 seconds by set FIL_LEN=80.
* To check the run time status of this software, you can run following command:
    ```
    hashpipe_status_monitor.rb
    or run this command: statm
    ```
### Settings
* Collapse data in time:
	set N_POST_VACC in FAST_databuf.h to perform post vaccumulation, add given number of spectrums together
* Collapse data in spectrum:
	set N_POST_CHANS_COMB in FAST_databuf.h to combine given number of channels together, this value must be set to 2^n
* Roaches concurrent data collection mechanism:
	use Redis database to set a start flag, check the value of flag to start collecting data.
* Multicast network:
	subscribe the associated socket to a multicast group, on FAST setifrb machine, the frb_group is set to 239.1.beam_number.1, beam_number is start from 1.
