/*FAST_gpu_thread.c
 *
 * 
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

/*abstract polarization (I,Q,U,V) form Original Data, according to data type information.

        I = X*X + Y*Y
        Q = X*X - Y*Y
        U = 2 * XY_real
        V = -2 * XY_img
*/
    
    int	 block_in  = binfo.in_block_idx;
    polar_data_t data;

    if (data_type == 0)
    {
	/*uint16_t uint16_test = 121;
	uint8_t uint8_test = (uint16_t)uint16_test;
	printf("uint8_test is : %d\n",uint8_test);
	*/	
       for(int i=0;i<N_SPEC_BUFF/N_POST_VACC;i++)
       //for(int i=0;i<N_SPEC_BUFF;i++)
          {
        	for(int j=0;j<N_CHANS_SPEC;j++)
		{	
			uint16_t post_vacc_tmp=0;
			for(int k=0;k<N_POST_VACC;k++)
			{
           			post_vacc_tmp  += 
        		(db_in->block[block_in].data[((i+1)*N_CHANS_SPEC-j-1)*N_POLS_PKT*N_POST_VACC+k*N_POLS_PKT]
        		+db_in->block[block_in].data[((i+1)*N_CHANS_SPEC-j-1)*N_POLS_PKT*N_POST_VACC+k*N_POLS_PKT+1]);
			//printf("data.Polar1[%d] equal db_in->block[block_in].data[%d]+db_in->block[block_in].data[%d]/%d/%d\n",i*N_CHANS_SPEC+j,((i+1)*N_CHANS_SPEC-j-1)*N_POLS_PKT*N_POST_VACC+k*N_POLS_PKT,((i+1)*N_CHANS_SPEC-j-1)*N_POLS_PKT*N_POST_VACC+k*N_POLS_PKT+1,N_POLS_PKT,N_POST_VACC);
			}
			data.Polar1[i*N_CHANS_SPEC+j] = (uint8_t)(post_vacc_tmp/N_POLS_PKT/N_POST_VACC);	
			//data.Polar1[i*N_CHANS_SPEC+j] = (uint8_t)((db_in->block[block_in].data[((i+1)*N_CHANS_SPEC-j-1)*N_POLS_PKT]+db_in->block[block_in].data[((i+1)*N_CHANS_SPEC-j-1)*N_POLS_PKT+1])/N_POLS_PKT);	
        	}
        }
    }
    //if (data_type == 0)
    //{	
    //   for(int j=0;j<N_CHANS_BUFF;j++)
    //      {
    //       data.Polar1[j]  = 
    //    	        db_in->block[block_in].data[j*N_POLS_PKT] 
    //    	      + db_in->block[block_in].data[j*N_POLS_PKT+1];

    //       data.Polar2[j]  = 
    //    		db_in->block[block_in].data[j*N_POLS_PKT]
    //    	      - db_in->block[block_in].data[j*N_POLS_PKT+1];
    //      }

    //    	
    //}
    //else if (data_type = 1)
    //{	
    //   for(int j=0;j<N_CHANS_BUFF;j++)
    //    
    //      {

    //       data.Polar1[j] = 
    //    		db_in->block[block_in].data[j*N_POLS_PKT]
    //    	      + db_in->block[block_in].data[j*N_POLS_PKT+1];

    //       data.Polar2[j] = 
    //    		db_in->block[block_in].data[j*N_POLS_PKT]
    //    	      - db_in->block[block_in].data[j*N_POLS_PKT+1];
    //      }
    //}
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

//	uint64_t netmcnt    = db_in->block[binfo.in_block_idx].header.netmcnt;
        hashpipe_status_lock_safe(&st);
        hputi4(st.buf, "COVT-IN", binfo.in_block_idx);
        hputs(st.buf, status_key, "waiting");
        hputi4(st.buf, "COVT-OUT", binfo.out_block_idx);
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
    skey: "COV-STAT",
    init: NULL,
    run:  run,
    ibuf_desc: {FAST_input_databuf_create},
    obuf_desc: {FAST_output_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&FAST_gpu_thread);
}

