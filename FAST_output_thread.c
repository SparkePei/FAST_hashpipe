/*
 * FAST_output_thread.c
 * 
 */
//#include <stdlib.h>
//#include <cstdio>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "hashpipe.h"
#include "FAST_databuf.h"
#include "filterbank.h"
#include <sys/time.h>
//#include "FAST_net_thread.h"
//#include "FAST_net_thread.c"
//static int block_idx=0;
extern int beam_ID;
extern bool data_type;
extern bool start_file;
extern double net_MJD;
static void *run(hashpipe_thread_args_t * args)
{
	printf("\n%f Mbytes for each Filterbank file.\n ",float(N_BYTES_PER_FILE)/1024/1024);
	printf("\n%d Channels per Buff.\n ",N_CHANS_BUFF);
	// Local aliases to shorten access to args fields
	// Our input buffer happens to be a FAST_ouput_databuf
	FAST_output_databuf_t *db = (FAST_output_databuf_t *)args->ibuf;
	hashpipe_status_t st = args->st;
	const char * status_key = args->thread_desc->skey;
	int rv, N_files;
	int block_idx = 0;
	uint64_t N_Bytes_save = 0;
	uint64_t N_Bytes_file = N_BYTES_PER_FILE;
	int filb_flag = 1;
	FILE * FAST_file_Polar_1;
	//FILE * FAST_file_Polar_2;
	char f_fil_P1[250];
	//char f_fil_P2[250];

	sleep(1);
	/* Main loop */
	while (run_threads()) {
		hashpipe_status_lock_safe(&st);
		hputi4(st.buf, "OUTBLKIN", block_idx);
		hputi8(st.buf, "DATSAVMB",(N_Bytes_save/1024/1024));
		hputi4(st.buf, "NFILESAV",N_files);
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
			struct tm  *now;
			time_t rawtime;
			char P[4] = {'I','Q','U','V'};
			printf("\n\nopen new filterbank file...\n\n");
			char File_dir[] = "/mnt/fast_frb_data/B";
			char t_stamp[50];
	        	time(&rawtime);
			now = localtime(&rawtime);
		        strftime(t_stamp,sizeof(t_stamp), "_%Y-%m-%d_%H-%M-%S.fil.working",now);

                        if (data_type ==0 ){
	                        sprintf(f_fil_P1,"%s%d%s%c%s" ,File_dir,beam_ID,"_",P[0],t_stamp);
	                        //sprintf(f_fil_P2,"%s%d%s%c%s" ,File_dir,beam_ID,"_",P[1],t_stamp);
			}
		//	else if(data_type ==1 ){
		//	        sprintf(f_fil_P1,"%s%d%s%c%s" ,File_dir,beam_ID,"_",P[2],t_stamp);
	        //                sprintf(f_fil_P2,"%s%d%s%c%s" ,File_dir,beam_ID,"_",P[3],t_stamp);
		//	}
			
			WriteHeader(f_fil_P1,net_MJD);
			//WriteHeader(f_fil_P2);
	
			printf("write header done!\n");
	
			N_files += 1;
			FAST_file_Polar_1=fopen(f_fil_P1,"a+");
			//FAST_file_Polar_2=fopen(f_fil_P2,"a+");
	
		        printf("starting write data to %s\n",f_fil_P1);
		        //printf("starting write data to %s \nand  %s...\n",f_fil_P1,f_fil_P2);
		}
	
                fwrite(db->block[block_idx].data.Polar1,sizeof(db->block[block_idx].data.Polar1),1,FAST_file_Polar_1);
                //fwrite(db->block[block_idx].data.Polar2,sizeof(db->block[block_idx].data.Polar2),1,FAST_file_Polar_2);
		N_Bytes_save += BUFF_SIZE/N_POLS_PKT;		
	
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
                        //char Filname_P2[250]={""};
                        strncpy(Filname_P1, f_fil_P1, strlen(f_fil_P1)-8);
                        //strncpy(Filname_P2, f_fil_P2, strlen(f_fil_P2)-8);
                        rename(f_fil_P1,Filname_P1);
                        //rename(f_fil_P2,Filname_P2);
			}

		else{
			filb_flag = 0;

			}		

		FAST_output_databuf_set_free(db,block_idx);
		block_idx = (block_idx + 1) % db->header.n_block;
		

		//Will exit if thread has been cancelled
		pthread_testcancel();

	}
	fclose(FAST_file_Polar_1);
	//fclose(FAST_file_Polar_2);
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

