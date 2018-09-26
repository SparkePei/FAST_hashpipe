/*FAST_gpu_thread.c
 * merge two pols together, do pol0 + pol1
 * collapse in time and frequency
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include "hashpipe.h"
#include "FAST_databuf.h"
#include "math.h"

static const char * status_key;
extern bool data_type;

typedef struct {
	bool        initialized;
	int         out_block_idx;
	int 	in_block_idx;
} cov_block_info_t;

static inline void initialize_block_info(cov_block_info_t * binfo)
{

	// If this block_info structure has already been initialized
	if(binfo->initialized) {
		return;
	}
	binfo->in_block_idx     = 0;
	binfo->out_block_idx    = 0;
	binfo->initialized	    = 1;
}

static cov_block_info_t binfo;

polar_data_t  polarization_process(FAST_input_databuf_t *db_in)
{
	int	 block_in  = binfo.in_block_idx;
	polar_data_t data;
	
	if (data_type == 0)
	{
		for(int i=0;i<N_SPEC_BUFF/N_POST_VACC;i++)
		{
 			for(int j=0;j<N_POST_CHANS_SPEC;j++)
			{	
				uint16_t post_vacc_tmp=0;
				// processing post vacc, add multiple spectrum togeter
				for(int k=0;k<N_POST_VACC;k++)
				{
					// processing post channelization, merge multiple channels to one
					for(int l=0;l<N_POST_CHANS_COMB;l++)
					{
           					post_vacc_tmp  += 
        				(db_in->block[block_in].data[i*N_POST_CHANS_SPEC*N_POLS_PKT*N_POST_VACC*N_POST_CHANS_COMB+((k+1)*N_POST_CHANS_SPEC*N_POST_CHANS_COMB-(j+1)*N_POST_CHANS_COMB+l)*N_POLS_PKT]
        				+db_in->block[block_in].data[i*N_POST_CHANS_SPEC*N_POLS_PKT*N_POST_VACC*N_POST_CHANS_COMB+((k+1)*N_POST_CHANS_SPEC*N_POST_CHANS_COMB-(j+1)*N_POST_CHANS_COMB+l)*N_POLS_PKT+1]);
					}
				}
				data.Polar1[i*N_POST_CHANS_SPEC+j] = (uint8_t)(post_vacc_tmp/N_POLS_PKT/N_POST_VACC/N_POST_CHANS_COMB); //average after post vacc and channelization
			}
		}
	}
	return data;
}

static void *run(hashpipe_thread_args_t * args)
{
	// Local aliases to shorten access to args fields
	FAST_input_databuf_t *db_in = (FAST_input_databuf_t *)args->ibuf;
	FAST_output_databuf_t *db_out = (FAST_output_databuf_t *)args->obuf;
	hashpipe_status_t st = args->st;
	status_key = args->thread_desc->skey;
	polar_data_t data;
	int rv;
	
	if(!binfo.initialized) {
		initialize_block_info(&binfo);
	}
	while (run_threads()) {
        	hashpipe_status_lock_safe(&st);
        	hputi4(st.buf, "GPUBLKIN", binfo.in_block_idx);
        	hputs(st.buf, status_key, "waiting");
        	hputi4(st.buf, "GPUBLKOUT", binfo.out_block_idx);
        	hashpipe_status_unlock_safe(&st);
        	// Wait for new input block to be filled
        	while ((rv=FAST_input_databuf_wait_filled(db_in, binfo.in_block_idx)) != HASHPIPE_OK) {
			if (rv==HASHPIPE_TIMEOUT) {
				hashpipe_status_lock_safe(&st);
				hputs(st.buf, status_key, "blocked");
				hashpipe_status_unlock_safe(&st);
				continue;
			} else {
				hashpipe_error(__FUNCTION__, "error waiting for filled databuf");
				pthread_exit(NULL);
				break;
			}
        	}
		// Note processing status
		hashpipe_status_lock_safe(&st);
		hputs(st.buf, status_key, "processing");
		hashpipe_status_unlock_safe(&st);
		// Process Data with polarization
		data = polarization_process(db_in);
		// Mark input block as free and advance
		FAST_input_databuf_set_free(db_in, binfo.in_block_idx);
		binfo.in_block_idx = (binfo.in_block_idx + 1) % db_in->header.n_block;
		// Wait for new output block to be free
		while ((rv=FAST_output_databuf_wait_free(db_out, binfo.out_block_idx)) != HASHPIPE_OK) {
			if (rv==HASHPIPE_TIMEOUT) {
    				hashpipe_status_lock_safe(&st);
    				hputs(st.buf, status_key, "block_out");
    				hashpipe_status_unlock_safe(&st);
    				continue;
    			} else {
        			hashpipe_error(__FUNCTION__, "error waiting for free databuf");
				pthread_exit(NULL);
				break;
			}
		}
		/*Copy Data to buffer*/
		memcpy(&db_out->block[binfo.out_block_idx].data,&data,sizeof(polar_data_t));
		if (TEST){
			fprintf(stderr,"**Net tread**\n");
			fprintf(stderr,"wait for output writting..\n");
			fprintf(stderr,"Data size: %lu \n\n",sizeof(data));
		}
        	// Mark output block as full and advance
        	FAST_output_databuf_set_filled(db_out,binfo.out_block_idx);
        	binfo.out_block_idx = (binfo.out_block_idx + 1) % db_out->header.n_block;		
        	/* Check for cancel */
        	pthread_testcancel();
	}
	return THREAD_OK;
}

static hashpipe_thread_desc_t FAST_gpu_thread = {
	name: "FAST_gpu_thread",
	skey: "GPUSTAT",
	init: NULL,
	run:  run,
	ibuf_desc: {FAST_input_databuf_create},
	obuf_desc: {FAST_output_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
	register_hashpipe_thread(&FAST_gpu_thread);
}

