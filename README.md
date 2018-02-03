# FAST_Pipeline_Hashpipe
This is thread manage pipeline for FAST FRB backend. 
The data are store in a Ram for temporary storage. When found a candidate, the data will be store into Disk.
This part include the former packet receiving, dissambling, and Filterbank data formate convertion. It has 3 threads:
  FAST_net_thread; (Packet receving)
  FAST_gpu_thread; ("gpu" is just a tradition to use, it has no GPU at all! Charge for stocks parameter re-assemble)
  FAST_output_thread; (Output data into RAM)
 
There are 2 buffers between three threads, each of them is a 3 segment ring buffer. Buffer status could be abstracted from each ring buffer.

Required:
  Hashpipe
  Ruby 2.1.2
  Install information: See:
  https://github.com/peterniuzai/Work_memo/blob/master/hashpipe_install.pdf

Make file  
  This pipeline including c for hashpipe and c++ for filterbank data formate convert(Written by K.J.Li).
  ./make_cmd  
  sudo make install
  
Hashpiepe has a monitor to waitch the system. 
  It is wrriten in Ruby, we could find the hashpipe monitor from 
  https://github.com/david-macmahon/rb-hashpipe.git
  
  After install hashpipe, if you want to run the monitor, you can put hashpipe_status_monitor.rb in terminal.
  

# FAST_hashpipe
# FAST_hashpipe
