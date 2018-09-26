/*
 * FAST_output_thread.c
 * Using local time for filename and MJD time in filterbank file's header
 * Using UTC time for Redis database
 * Getting Accumulation length from Redis database for Tsamp setting and update in every file.
 */
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "hashpipe.h"
#include "FAST_databuf.h"
#include <hiredis/hiredis.h>
#include "filterbank.h"
#include <sys/time.h>
extern int beam_ID;
extern bool data_type;
extern bool start_file;
extern double net_MJD;
int acc_len;
double samp_time;

extern double UTC2JD(double year, double month, double day);

static void *run(hashpipe_thread_args_t * args)
{
	double n_bytes_per_fil;
	int fil_len;
	
	//redis database initialize
	redisContext *redis_c;
	redisReply *reply;
	const char *redis_host = "10.128.1.99";
	int redis_port = 6379;
	struct timeval timeout = { 1, 500000 }; // 0.5 seconds
	redis_c = redisConnectWithTimeout(redis_host, redis_port, timeout);
	if (redis_c == NULL || redis_c->err) {
	    if (redis_c) {
	        printf("Connection error: %s\n", redis_c->errstr);
	        redisFree(redis_c);
	    } else {
	        printf("Connection error: can't allocate redis context\n");
	    }
	    exit(1);
	}
	
	// get accumulation length from redis database
	reply = (redisReply *)redisCommand(redis_c,"GET fpga_acc_len");
	acc_len = 1 + atoi(reply->str); // acc_len start from 0 in FPGA

	// Local aliases to shorten access to args fields
	// Our input buffer happens to be a FAST_ouput_databuf
	FAST_output_databuf_t *db = (FAST_output_databuf_t *)args->ibuf;
	hashpipe_status_t st = args->st;
	const char * status_key = args->thread_desc->skey;
	int rv;
	int N_files=0;
	int block_idx = 0;
	uint64_t N_Bytes_save = 0;
	uint64_t N_Bytes_file = 0;
	int filb_flag = 1;
	FILE * FAST_file_Polar_1;
	FILE * csv_file;
	char f_fil_P1[250];
	struct tm  *now,*UTC_time;
	time_t rawtime;
       	double Year, Month, Day,jd;
        struct timeval currenttime;
	char P[4] = {'I','Q','U','V'};
	char File_dir[] = "/dataramdisk/fast_frb_data/B";
	char t_stamp[50];
	char csv_t_start[20], csv_t_end[20];
	char csv_string[100];
	char  comp_node[4];
	int n_size_name;
	// get observation time length from setting
	hashpipe_status_lock_safe(&st);
	hgeti4(st.buf, "FIL_LEN", &fil_len);
	hashpipe_status_unlock_safe(&st);
	// calculate sampling time and file size
	samp_time = (FFT_CHANS*2.0*acc_len/CLOCK*1.0e-6);
	n_bytes_per_fil = (fil_len/samp_time/N_POST_VACC*N_CHANS_SPEC/N_POST_CHANS_COMB);                  // only save (I+Q)/2 into disk.
	N_Bytes_file = n_bytes_per_fil;
	printf("\n%f Mbytes for each Filterbank file.\n ",float(n_bytes_per_fil)/1024/1024);
	printf("\n%d Channels per Buff.\n ",N_CHANS_BUFF/N_POST_VACC/N_POST_CHANS_COMB);
	n_size_name=gethostname(comp_node,sizeof(comp_node));
	printf("compute node:%s\n",comp_node);
	sleep(1);
	/* Main loop */
	while (run_threads()) {
		hashpipe_status_lock_safe(&st);
		hputi4(st.buf, "OUTBLKIN", block_idx);
		hputr8(st.buf, "MJD(loc", net_MJD);
		hputi8(st.buf, "FILSIZMB",(N_Bytes_file/1024/1024));
		hputi8(st.buf, "DATSAVMB",(N_Bytes_save/1024/1024));
		hputi4(st.buf, "NFILESAV",N_files);
		hputr4(st.buf,"TSAMP(ms)",samp_time*N_POST_VACC*1000);
		hputs(st.buf, status_key, "waiting");
		hashpipe_status_unlock_safe(&st);

		// Wait for data to storage
		while ((rv=FAST_output_databuf_wait_filled(db, block_idx))
		!= HASHPIPE_OK) {
			if (rv==HASHPIPE_TIMEOUT) {
				hashpipe_status_lock_safe(&st);
				hputs(st.buf, status_key, "blocked");
				hputi4(st.buf, "OUTBLKIN", block_idx);
				hashpipe_status_unlock_safe(&st);
				continue;
			} else {
				hashpipe_error(__FUNCTION__, "error waiting for filled databuf");
				pthread_exit(NULL);
				break;
			}
		}
		
		hashpipe_status_lock_safe(&st);
		hputs(st.buf, status_key, "processing");
		hputi4(st.buf, "OUTBLKIN", block_idx);
		hashpipe_status_unlock_safe(&st);
		if (filb_flag ==1 && start_file ==1 ){
			printf("\nopen new filterbank file...\n");
			// update time every file
	        	time(&rawtime);
			now = localtime(&rawtime);
        		Year=now->tm_year+1900;
        		Month=now->tm_mon+1;
        		Day=now->tm_mday;
        		jd = UTC2JD(Year, Month, Day);
        		net_MJD=jd+(double)((now->tm_hour-12)/24.0)// UTC time
        		                       +(double)(now->tm_min/1440.0)
        		                       +(double)(now->tm_sec/86400.0)
        		                       +(double)(currenttime.tv_usec/86400.0/1000000.0)
        		                        -(double)2400000.5;

		        strftime(t_stamp,sizeof(t_stamp), "_%Y-%m-%d_%H-%M-%S.fil.working",now);
	        	time(&rawtime);
			UTC_time = gmtime(&rawtime);	
			strftime(csv_t_start,sizeof(csv_t_start),"%Y-%m-%d-%H:%M:%S",UTC_time);

                        if (data_type ==0 ){
				 sprintf(f_fil_P1,"%s%d%s%c%s" ,File_dir,beam_ID,"_",P[0],t_stamp);
    				// update accumulation length for every file
    				reply = (redisReply *)redisCommand(redis_c,"GET fpga_acc_len");
    				acc_len = 1 + atoi(reply->str); // acc_len start from 0 in FPGA
				// calculate sampling time and file size
				samp_time = (FFT_CHANS*2.0*acc_len/CLOCK*1.0e-6);
				n_bytes_per_fil = (fil_len/samp_time/N_POST_VACC*N_CHANS_SPEC/N_POST_CHANS_COMB);                  // only save (I+Q)/2 into disk.
				N_Bytes_file = n_bytes_per_fil;
			}
			WriteHeader(f_fil_P1,net_MJD);
			printf("write header done!\n");
			N_files += 1;
			FAST_file_Polar_1=fopen(f_fil_P1,"a+");
			printf("starting write data to %s\n",f_fil_P1);
			//printf("starting write data to %s \nand  %s...\n",f_fil_P1,f_fil_P2);
		}
                fwrite(db->block[block_idx].data.Polar1,sizeof(db->block[block_idx].data.Polar1),1,FAST_file_Polar_1);
		N_Bytes_save += BUFF_SIZE/N_POLS_PKT/N_POST_VACC/N_POST_CHANS_COMB;		
	
		if (TEST){

			printf("**Save Information**\n");
			printf("beam_ID:%d \n",beam_ID);
			printf("Buffsize: %lu",BUFF_SIZE);
			printf("flib_flag:%d\n",filb_flag);
			printf("Data save:%f\n",float(N_Bytes_save)/1024/1024);
			printf("Total file size:%f\n",float(N_Bytes_file)/1024/1024);
			printf("Devide:%lu\n\n",N_Bytes_save % N_Bytes_file);
		}

		if (N_Bytes_save >= N_Bytes_file){

			filb_flag = 1;
			N_Bytes_save = 0;
                        char Filname_P1[250]={""};
			char redis_cmd[100];
			char fn_redis[250];
                        strncpy(Filname_P1, f_fil_P1, strlen(f_fil_P1)-8);
			fclose(FAST_file_Polar_1);
                        rename(f_fil_P1,Filname_P1);
			// write filename, start time, end time, beam number to csv file
	        	time(&rawtime);
			UTC_time = gmtime(&rawtime); //	get UTC time	
			strftime(csv_t_end,sizeof(csv_t_end),"%Y-%m-%d-%H:%M:%S",UTC_time);
			sprintf(csv_string,"%s%c%s%c%s%c%d\n",Filname_P1,',',csv_t_start,',',csv_t_end,',',beam_ID);
			// extract file name, no directory
                        strncpy(fn_redis,Filname_P1+27, strlen(Filname_P1)-27);
			sprintf(redis_cmd,"%s%s%s%s" ,"RPUSH ", comp_node," ",fn_redis);
    			reply = (redisReply *)redisCommand(redis_c,redis_cmd);
			sprintf(redis_cmd,"%s%s%s%d" ,"EXPIRE ",comp_node," ",86400);
    			reply = (redisReply *)redisCommand(redis_c,redis_cmd);
			sprintf(redis_cmd,"%s%s%s%s" ,"RPUSH ",fn_redis," ",csv_t_start);
    			reply = (redisReply *)redisCommand(redis_c,redis_cmd);
			sprintf(redis_cmd,"%s%s%s%s" ,"RPUSH ",fn_redis," ",csv_t_end);
    			reply = (redisReply *)redisCommand(redis_c,redis_cmd);
			sprintf(redis_cmd,"%s%s%s%d" ,"RPUSH ",fn_redis," ",beam_ID);
    			reply = (redisReply *)redisCommand(redis_c,redis_cmd);
			sprintf(redis_cmd,"%s%s%s%d" ,"RPUSH ",fn_redis," ",-1);
    			reply = (redisReply *)redisCommand(redis_c,redis_cmd);
			sprintf(redis_cmd,"%s%s%s%d" ,"EXPIRE ",fn_redis," ",86400);
    			reply = (redisReply *)redisCommand(redis_c,redis_cmd);
		    	freeReplyObject(reply);
		}

		else{
			filb_flag = 0;

		}		
		FAST_output_databuf_set_free(db,block_idx);
		block_idx = (block_idx + 1) % db->header.n_block;

		//Will exit if thread has been cancelled
		pthread_testcancel();
	}
	return THREAD_OK;
}

static hashpipe_thread_desc_t FAST_output_thread = {
	name: "FAST_output_thread",
	skey: "OUTSTAT",
	init: NULL, 
	run:  run,
	ibuf_desc: {FAST_output_databuf_create},
	obuf_desc: {NULL}
};

static __attribute__((constructor)) void ctor()
{
	register_hashpipe_thread(&FAST_output_thread);
}

