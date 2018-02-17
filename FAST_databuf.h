#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "hashpipe.h"
#include "hashpipe_databuf.h"


#define CACHE_ALIGNMENT         64
#define N_INPUT_BLOCKS          3 
#define N_OUTPUT_BLOCKS         3
#define TEST			0

#define N_BEAM			19
#define N_POST_VACC		4			//number of post vaccumulation, how many spectrums added together
#define N_CHAN_PER_PACK		2048			//Number of channels per packet
#define N_PACKETS_PER_SPEC	2			//Number of packets per spectrum
#define N_BYTES_DATA_POINT	1			//Number of bytes per datapoint
#define N_POLS_PKT		2			//Number of polarizations per packet
#define N_BYTES_HEADER		8			//Number of Bytes of header
#define N_SPEC_BUFF             (512*N_POST_VACC)		//Number of spectrums per buffer
#define N_BITS_DATA_POINT       (N_BYTES_DATA_POINT*8) 	//Number of bits per datapoint in packet
#define N_CHANS_SPEC		(N_CHAN_PER_PACK * N_PACKETS_PER_SPEC) 					//Channels in spectrum for 1 pole.
#define DATA_SIZE_PACK		(unsigned long)(N_CHAN_PER_PACK * N_POLS_PKT *  N_BYTES_DATA_POINT) 	//Packet size without Header 
#define PKTSIZE			(DATA_SIZE_PACK + N_BYTES_HEADER)					//Total Packet size 
#define N_BYTES_PER_SPEC	(DATA_SIZE_PACK*N_PACKETS_PER_SPEC)					//Spectrum size with polarations
#define BUFF_SIZE		(unsigned long)(N_SPEC_BUFF*N_BYTES_PER_SPEC) 				//Buffer size with polarations
#define N_CHANS_BUFF		(N_SPEC_BUFF*N_CHANS_SPEC)     						//Channels in one buffer without polarations
//#define N_SPEC_PER_FILE		1199616/4 			// Number of spectrums per file \
				int{time(s)/T_samp(s)/N_SPEC_BUFF}*N_SPEC_BUFF  e.g. 20s data: int(20/0.001/128)*128
//#define N_BYTES_PER_FILE	(N_SPEC_PER_FILE * N_BYTES_PER_SPEC / N_POLS_PKT) 			// we can save (I,Q,U,V) polaration into disk. 
#define SAMP_TIME		256e-6			// sec
#define FIL_LEN			20			// sec
#define N_BYTES_PER_FILE	(FIL_LEN/SAMP_TIME/N_POST_VACC*N_CHANS_SPEC) 			// we can save (I,Q,U,V) polaration into disk. 



// Used to pad after hashpipe_databuf_t to maintain cache alignment
typedef uint8_t hashpipe_databuf_cache_alignment[
  CACHE_ALIGNMENT - (sizeof(hashpipe_databuf_t)%CACHE_ALIGNMENT)
];

//Define Stocks Parameter I.Q.U.V. Data structure
typedef struct polar_data {

   uint8_t Polar1[N_CHANS_BUFF/N_POST_VACC];
//   uint8_t Polar2[N_CHANS_BUFF];

}polar_data_t;


/* INPUT BUFFER STRUCTURES*/
typedef struct FAST_input_block_header {
   uint64_t	netmcnt;        // Counter for ring buffer
   		
} FAST_input_block_header_t;

typedef uint8_t FAST_input_header_cache_alignment[
   CACHE_ALIGNMENT - (sizeof(FAST_input_block_header_t)%CACHE_ALIGNMENT)
];

typedef struct FAST_input_block {

   FAST_input_block_header_t header;
   FAST_input_header_cache_alignment padding; // Maintain cache alignment
   uint8_t  data[N_CHANS_BUFF * N_POLS_PKT]; //Input buffer for all channels

} FAST_input_block_t;

typedef struct FAST_input_databuf {
   hashpipe_databuf_t header;
   hashpipe_databuf_cache_alignment padding; // Maintain cache alignment
   FAST_input_block_t block[N_INPUT_BLOCKS];
} FAST_input_databuf_t;


/*
  * OUTPUT BUFFER STRUCTURES
  */
typedef struct FAST_output_block_header {

} FAST_output_block_header_t;

typedef uint8_t FAST_output_header_cache_alignment[
   CACHE_ALIGNMENT - (sizeof(FAST_output_block_header_t)%CACHE_ALIGNMENT)
];

typedef struct FAST_output_block {

   FAST_output_block_header_t header;
   FAST_output_header_cache_alignment padding; // Maintain cache alignment
   polar_data_t data;

} FAST_output_block_t;

typedef struct FAST_output_databuf {
   hashpipe_databuf_t header;
   hashpipe_databuf_cache_alignment padding; // Maintain cache alignment
   FAST_output_block_t block[N_OUTPUT_BLOCKS];
} FAST_output_databuf_t;

/*
 * INPUT BUFFER FUNCTIONS
 */
hashpipe_databuf_t *FAST_input_databuf_create(int instance_id, int databuf_id);

static inline FAST_input_databuf_t *FAST_input_databuf_attach(int instance_id, int databuf_id)
{
    return (FAST_input_databuf_t *)hashpipe_databuf_attach(instance_id, databuf_id);
}

static inline int FAST_input_databuf_detach(FAST_input_databuf_t *d)
{
    return hashpipe_databuf_detach((hashpipe_databuf_t *)d);
}

static inline void FAST_input_databuf_clear(FAST_input_databuf_t *d)
{
    hashpipe_databuf_clear((hashpipe_databuf_t *)d);
}

static inline int FAST_input_databuf_block_status(FAST_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_block_status((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_input_databuf_total_status(FAST_input_databuf_t *d)
{
    return hashpipe_databuf_total_status((hashpipe_databuf_t *)d);
}

static inline int FAST_input_databuf_wait_free(FAST_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_wait_free((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_input_databuf_busywait_free(FAST_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_busywait_free((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_input_databuf_wait_filled(FAST_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_wait_filled((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_input_databuf_busywait_filled(FAST_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_busywait_filled((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_input_databuf_set_free(FAST_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_set_free((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_input_databuf_set_filled(FAST_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_set_filled((hashpipe_databuf_t *)d, block_id);
}

/*
 * OUTPUT BUFFER FUNCTIONS
 */

hashpipe_databuf_t *FAST_output_databuf_create(int instance_id, int databuf_id);

static inline void FAST_output_databuf_clear(FAST_output_databuf_t *d)
{
    hashpipe_databuf_clear((hashpipe_databuf_t *)d);
}

static inline FAST_output_databuf_t *FAST_output_databuf_attach(int instance_id, int databuf_id)
{
    return (FAST_output_databuf_t *)hashpipe_databuf_attach(instance_id, databuf_id);
}

static inline int FAST_output_databuf_detach(FAST_output_databuf_t *d)
{
    return hashpipe_databuf_detach((hashpipe_databuf_t *)d);
}

static inline int FAST_output_databuf_block_status(FAST_output_databuf_t *d, int block_id)
{
    return hashpipe_databuf_block_status((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_output_databuf_total_status(FAST_output_databuf_t *d)
{
    return hashpipe_databuf_total_status((hashpipe_databuf_t *)d);
}

static inline int FAST_output_databuf_wait_free(FAST_output_databuf_t *d, int block_id)
{
    return hashpipe_databuf_wait_free((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_output_databuf_busywait_free(FAST_output_databuf_t *d, int block_id)
{
    return hashpipe_databuf_busywait_free((hashpipe_databuf_t *)d, block_id);
}
static inline int FAST_output_databuf_wait_filled(FAST_output_databuf_t *d, int block_id)
{
    return hashpipe_databuf_wait_filled((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_output_databuf_busywait_filled(FAST_output_databuf_t *d, int block_id)
{
    return hashpipe_databuf_busywait_filled((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_output_databuf_set_free(FAST_output_databuf_t *d, int block_id)
{
    return hashpipe_databuf_set_free((hashpipe_databuf_t *)d, block_id);
}

static inline int FAST_output_databuf_set_filled(FAST_output_databuf_t *d, int block_id)
{
    return hashpipe_databuf_set_filled((hashpipe_databuf_t *)d, block_id);
}


