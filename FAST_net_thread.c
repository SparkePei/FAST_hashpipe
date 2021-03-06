/*
 * FAST_net_thread.c
 *
 * Receive packets from ten Gb port of ROACH2, following is the detailed information.
 * # Packets format from FRB interface which p2p1 for power and p2p2 for cross term. each packet has 4104 bytes, 8 bytes header and 4096 bytes data.
 * # Each spectrum is divided into 2 UDP packets, distinguish by the last bit of counter number.
 *
 * # Following is the format of single spectrum 
 * #               |    bin 0    |    bin 1    |      |  bin 2047     |
 * #pkt0: | header | pol0 | pol1 | pol0 | pol1 | .... | pol 0 | pol 1 |
 * #               | bin 2048    | bin 2049    |      |  bin 4095     |
 * #pkt1: | header | pol0 | pol1 | pol0 | pol1 | .... | pol 0 | pol 1 |
 *
 * # Following is the format of 8 bytes header
 * # | byte 7      |  byte 6 -- 0  |
 * # | source ID   |  counter      |
 *
 * # Following is the format of source ID
 * # |     bit 7           |     bit 6     |  bit 5 -- 1    |  bit 0       |
 * # |0-power spectra,     |0 - single beam|multi-beam:1-19 |0 - power term|
 * # |1-pure ADC sample    |1 - multi-beam |single-beam: TBD|1 - cross term|
 *
 * Subscribe to multicasting group. 
 * Fill 0 when packets missing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include "hashpipe.h"
#include "FAST_databuf.h"
#include <hiredis/hiredis.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sched.h>

int beam_ID;
bool data_type=0;
bool start_file=0;
double net_MJD;
static int total_packets_counted = 0;
static hashpipe_status_t *st_p;
static  const char * status_key;

double UTC2JD(double year, double month, double day){
	double jd;
	double a;
	a = floor((14-month)/12);
	year = year+4800-a;
	month = month+12*a-3;
	jd = day + floor((153*month+2)/5)+365*year+floor(year/4)-floor(year/100)+floor(year/400)-32045;
	return jd;
}

static int init(hashpipe_thread_args_t * args)
{
	hashpipe_status_t st = args->st;
	hashpipe_status_lock_safe(&st);
	hputi8(st.buf,"BUFMCNT",0);
	hputi8(st.buf, "NPACKETS", 0);
	hputi8(st.buf, "DATSAVMB",0);
	hputi8(st.buf,"MiSSPKT",0);
	hashpipe_status_unlock_safe(&st);
	return 0;
}

typedef struct {
	uint64_t mcnt;            // counter for packet
	bool   	source_from;    // 0 - power spectra, 1 - pure ADC sample
	bool   	beam_type;      // 0 - single beam, 1 - multi-beam
	int	Beam_ID;        // beam ID
	bool	pkt_dtype;	// spectra: 0 - power term, 1 - cross term
} packet_header_t;


typedef struct {
	uint64_t	cur_mcnt;
	long 		miss_pkt;
	long   		offset;
	int         	initialized;
	int		block_idx;
	bool		start_flag;
} block_info_t;

static block_info_t binfo;
// This function must be called once and only once per block_info structure!
// Subsequent calls are no-ops.
static inline void initialize_block_info(block_info_t * binfo)
{
	// If this block_info structure has already been initialized
	if(binfo->initialized) {
		return;
	}
	
	binfo->cur_mcnt	= 0;
	binfo->block_idx = 0;
	binfo->start_flag = 0;
	binfo->offset = 0;
	binfo->miss_pkt	= 0;
	binfo->initialized = 1;
}

static inline void get_header( packet_header_t * pkt_header, char *packet)
{
	uint64_t raw_header;
	memcpy(&raw_header,packet,N_BYTES_HEADER*sizeof(char));
	pkt_header->mcnt        = raw_header  & 0x00ffffffffffffff; 
	pkt_header->source_from = raw_header  & 0x8000000000000000; //0 - power spectra, 1 - pure ADC sample
	pkt_header->beam_type   = raw_header  & 0x4000000000000000; //0 - single beam, 1 - multi-beam
	pkt_header->Beam_ID     = (raw_header >> 57)& 0x1f; //5 bits, 32 values for beam. multi-beam: 1-19
	pkt_header->pkt_dtype   = raw_header  & 0x0100000000000000; //spectra: 0 - power term, 1 - cross term
	if (TEST){
	    	fprintf(stderr,"**Header**\n");
	    	fprintf(stderr,"Mcnt of Header is :%lu \n ",pkt_header->mcnt);
	    	fprintf(stderr,"Raw Header: %lu \n",raw_header);
	    	fprintf(stderr,"Seq Number:%lu\n\n",pkt_header->mcnt%2);
	}
}

static inline void miss_pkt_process( uint64_t pkt_mcnt, FAST_input_databuf_t *db) 
{
	binfo.miss_pkt	+= (pkt_mcnt - binfo.cur_mcnt);
	long  miss_pkt       =  pkt_mcnt - binfo.cur_mcnt;
	uint64_t miss_size   =  miss_pkt * DATA_SIZE_PACK;
	int rv;
	if (((binfo.offset + miss_size ) >= BUFF_SIZE) && (miss_size < BUFF_SIZE))
	{
		while (( rv = FAST_input_databuf_wait_free(db, binfo.block_idx))!= HASHPIPE_OK) {
			if (rv==HASHPIPE_TIMEOUT) {
        	          	hashpipe_status_lock_safe(st_p);
        	          	hputs(st_p->buf, status_key, "blocked");
        	          	hashpipe_status_unlock_safe(st_p);
        	          	continue;
        	       	} else {
        	           	hashpipe_error(__FUNCTION__, "error waiting for free databuf");
        	           	pthread_exit(NULL);
        	           	break;
        	      	}
		}
		memset(db->block[binfo.block_idx].data+binfo.offset,0,(BUFF_SIZE - binfo.offset)*sizeof(char));
		binfo.offset  = binfo.offset + miss_size - BUFF_SIZE;//Give new offset after 1 buffer zero.
		
		// Mark block as full
		if(FAST_input_databuf_set_filled(db, binfo.block_idx) != HASHPIPE_OK) {
        	    	hashpipe_error(__FUNCTION__, "error waiting for databuf filled call");
        	    	pthread_exit(NULL);
		}
        	binfo.block_idx = (binfo.block_idx + 1) % db->header.n_block;
		while ((rv = FAST_input_databuf_wait_free(db, binfo.block_idx))!= HASHPIPE_OK) {
			if (rv==HASHPIPE_TIMEOUT) {
				hashpipe_status_lock_safe(st_p);
				hputs(st_p->buf, status_key, "blocked");
				hashpipe_status_unlock_safe(st_p);
				continue;
			} else {
				hashpipe_error(__FUNCTION__, "error waiting for free databuf");
				pthread_exit(NULL);
				break;
			}
		}
		memset(db->block[binfo.block_idx].data,0,(binfo.offset)*sizeof(char));
	}
	else if(miss_size > BUFF_SIZE){
		 printf("SYSTEM mcnt:%lu \n\n", binfo.cur_mcnt);
                 printf("Packet mcnt:%lu \n", pkt_mcnt);
		 printf("Miss_size:%lu \n",miss_size);
		 printf("BUFF_SIZE: %lu \n\n",BUFF_SIZE);
		 fprintf(stderr,"Missing Pkt much more than one Buffer...\n");
                 pthread_exit(NULL);
		 exit(1);
	}
	else{
		if(TEST){
			printf("**Miss packet! hooo no!**\n");
			printf("binfo mcnt:%lu \n", binfo.cur_mcnt);
	                printf("Packet mcnt:%lu \n\n", pkt_mcnt);
		}
		while (( rv = FAST_input_databuf_wait_free(db, binfo.block_idx))!= HASHPIPE_OK) {
			if (rv==HASHPIPE_TIMEOUT) {
				hashpipe_status_lock_safe(st_p);
				hputs(st_p->buf, status_key, "blocked");
				hashpipe_status_unlock_safe(st_p);
				continue;
			} else {
				hashpipe_error(__FUNCTION__, "error waiting for free databuf");
				pthread_exit(NULL);
				break;
			}
		}
		memset(db->block[binfo.block_idx].data,0,(miss_size)*sizeof(char));
	}
	binfo.cur_mcnt = pkt_mcnt + N_PACKETS_PER_SPEC - pkt_mcnt % N_PACKETS_PER_SPEC;
	
}

static inline void process_packet(FAST_input_databuf_t *db,char *packet)
{
	packet_header_t pkt_header;
	uint64_t pkt_mcnt = 0;
	int pkt_source	= 0;
	int seq         = 0;
	int pkt_bmtype	= 0;
	int pkt_beamID	= 0;
	int pkt_dtype	= 0;
	int rv		= 0;
	// Parse packet header
	get_header(&pkt_header,packet);
	pkt_mcnt	= pkt_header.mcnt;
	pkt_source	= pkt_header.source_from;
	pkt_bmtype	= pkt_header.beam_type;
	pkt_beamID	= pkt_header.Beam_ID;
	data_type	= pkt_header.pkt_dtype;
	seq =  pkt_mcnt % N_PACKETS_PER_SPEC;
	start_file  = 1;
	// Copy Header Information
	beam_ID = pkt_beamID; // beam number start from 1
	
	if(TEST){
		fprintf(stderr,"**Before start**\n");
		fprintf(stderr,"cur_mcnt: %lu \n",binfo.cur_mcnt);
		fprintf(stderr,"pkt_mcnt: %lu \n",pkt_mcnt);	
		fprintf(stderr,"seq :%d \n ",seq);
		fprintf(stderr,"beam :%d \n ",beam_ID);
		fprintf(stderr,"start flag :%d \n\n ",binfo.start_flag);
	}

	if (seq == 0 || binfo.start_flag ){
		if(TEST){printf("\n ********start !!!******\n\n");}
		if (total_packets_counted == 0 ){ 
			binfo.cur_mcnt = pkt_mcnt;
			printf("set two mcnt are the same to start.\n");
		}
		total_packets_counted++;
		if(binfo.cur_mcnt == pkt_mcnt){
			while (( rv = FAST_input_databuf_wait_free(db, binfo.block_idx))!= HASHPIPE_OK) {
				if (rv==HASHPIPE_TIMEOUT) {
				hashpipe_status_lock_safe(st_p);
				hputs(st_p->buf, status_key, "blocked");
				hashpipe_status_unlock_safe(st_p);
				continue;
				} else {
				hashpipe_error(__FUNCTION__, "error waiting for free databuf");
				pthread_exit(NULL);
				break;
				}
			}
			// Copy data into buffer
			memcpy((db->block[binfo.block_idx].data)+binfo.offset, packet+8, DATA_SIZE_PACK*sizeof(char));
			// Show Status of buffer
			hashpipe_status_lock_safe(st_p);
			hputi8(st_p->buf,"PKTseq",seq);
			hputi8(st_p->buf,"MiSSPKT",binfo.miss_pkt);
			hashpipe_status_unlock_safe(st_p);
			
			binfo.offset     += DATA_SIZE_PACK;
			binfo.start_flag  = 1;
			binfo.cur_mcnt   += 1;
			
				if (binfo.offset == BUFF_SIZE){
				if(TEST){fprintf(stderr,"\nOffset already buffsize!: %lu \n",binfo.offset);}
				// Mark block as full
				if(FAST_input_databuf_set_filled(db, binfo.block_idx) != HASHPIPE_OK) {
					hashpipe_error(__FUNCTION__, "error waiting for databuf filled call");
					pthread_exit(NULL);
				}
				binfo.block_idx = (binfo.block_idx + 1) % db->header.n_block;
				binfo.offset = 0;
				binfo.start_flag = 0;
			}
		}//if (binfo.cur_mcnt == pkt_mcnt)
		else{
			miss_pkt_process(pkt_mcnt, db);
			binfo.start_flag = 0;
		}
	}//(seq == 0 || binfo.start_flag )
	if(TEST){
		printf("\n ********End !!!******\n\n");
	}
}

static void *run(hashpipe_thread_args_t * args)
{
	total_packets_counted == 0;
	double Year, Month, Day;
	double jd;
	time_t timep;
	struct tm *p_t;
	struct timeval currenttime;
	redisContext *redis_c;
	redisReply *reply;
	const char *redis_host = "10.128.1.99";
	int redis_port = 6379;
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
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
	FAST_input_databuf_t *db  = (FAST_input_databuf_t *)args->obuf;
	hashpipe_status_t st = args->st;
	status_key = args->thread_desc->skey;
	st_p = &st; // allow global (this source file) access to the status buffer
	/*Start to receive data*/
	struct hashpipe_udp_params up;
	// turn bindhost into an IP address that we can use in bind()
	char frb_group[32];
	char  comp_node[4];
	int n_size_name,n_node;
	char host_number[2];
	n_size_name=gethostname(comp_node,sizeof(comp_node));
	strncpy(host_number,comp_node+1, 2);
	n_node=atoi(host_number);
	n_node=n_node-12;
	sprintf(frb_group,"239.1.%d.%d",n_node,1);
	strcpy(up.bindhost,frb_group);
	up.bindport = 12345;
	up.packet_size = PKTSIZE;
	sleep(1);   		
	struct hashpipe_udp_packet p;   
	hashpipe_status_lock_safe(&st);
	hputs(st.buf, "BINDHOST", up.bindhost);
	hputi4(st.buf, "BINDPORT", up.bindport);
	hputs(st.buf, status_key, "running");
	hashpipe_status_unlock_safe(&st);
	char *packet;
	packet = (char *)malloc(PKTSIZE*sizeof(char));
	int pkt_size;
	if(!binfo.initialized) {
		initialize_block_info(&binfo);
		db->block[binfo.block_idx].header.netmcnt=0;
		printf("\nInitailized!\n");
	}
	/* Give all the threads a chance to start before opening network socket */
	sleep(2);
    	/* Get receiving flag from redis server */
	printf("waiting for set start_flag to 1 on server to start ...\n");
	do {
    		reply = (redisReply *)redisCommand(redis_c,"GET start_flag");
		sleep(0.1);
	} while(strcmp(reply->str,"1")!=0); // if start_flag set to 1 then start data receiving.
	// wait until the integer time value changes.
	time_t time0=time(&timep);
	while(time(&timep)==time0);
	// start to collect data.
	printf("GET value from %s and start_flag is: %s, start data receiving...\n", redis_host, reply->str);
	freeReplyObject(reply);
	/* Set up UDP socket */
	int rv = hashpipe_udp_init(&up);
	struct ip_mreq mreq;
	struct ifreq ifr;
	struct sockaddr_in addr;
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name , "p2p1", IFNAMSIZ-1);
	ioctl(up.sock, SIOCGIFADDR, &ifr);
	char * bindhost_addr = inet_ntoa(( (struct sockaddr_in *)&ifr.ifr_addr )->sin_addr);
	addr.sin_addr.s_addr = inet_addr(bindhost_addr);
	// join the associate socket to the multicast group
	mreq.imr_multiaddr.s_addr = inet_addr(frb_group);
	mreq.imr_interface.s_addr = inet_addr(bindhost_addr);
	if (setsockopt(up.sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		hashpipe_error("FAST_net_thread", "Error joining multicast group.");
		pthread_exit(NULL);
	}

	if (rv!=HASHPIPE_OK) {
		hashpipe_error("FAST_net_thread",
		        "Error opening UDP socket.");
		pthread_exit(NULL);
	}
	/*Check first two block */
	while ((rv=FAST_input_databuf_wait_free(db, 0))
		        != HASHPIPE_OK) {
		if (rv==HASHPIPE_TIMEOUT) {
			hashpipe_status_lock_safe(&st);
			hputs(st.buf, status_key, "blocked");
			hashpipe_status_unlock_safe(&st);
			continue;
		} else {
			hashpipe_error(__FUNCTION__, "error waiting for free databuf");
			pthread_exit(NULL);
			break;
		}
	}
	while ((rv=FAST_input_databuf_wait_free(db, 1))
		!= HASHPIPE_OK) {
		if (rv==HASHPIPE_TIMEOUT) {
			hashpipe_status_lock_safe(&st);
			hputs(st.buf, status_key, "blocked");
			hashpipe_status_unlock_safe(&st);
			continue;
		} else {
			hashpipe_error(__FUNCTION__, "error waiting for free databuf");
			pthread_exit(NULL);
			break;
		}
	}
	
	// update MJD time use localtime
	time(&timep);
	p_t=localtime(&timep);
	Year=p_t->tm_year+1900;
	Month=p_t->tm_mon+1;
	Day=p_t->tm_mday;
	jd = UTC2JD(Year, Month, Day);
	net_MJD=jd+(double)((p_t->tm_hour-12)/24.0)// UTC time
	                       +(double)(p_t->tm_min/1440.0)
	                       +(double)(p_t->tm_sec/86400.0)
	                       +(double)(currenttime.tv_usec/86400.0/1000000.0)
	                            -(double)2400000.5;
	/* Main loop */
	while (run_threads()) {
		hashpipe_status_lock_safe(&st);
		hputs(st.buf, status_key, "running");
		hputi4(st.buf, "NETBKOUT", binfo.block_idx);
		hputi8(st.buf, "NPACKETS", total_packets_counted);
		hashpipe_status_unlock_safe(&st);
		pkt_size = recvfrom(up.sock,packet,PKTSIZE*sizeof(char),0,NULL,NULL);	
		if(!run_threads()) {break;}
		if (pkt_size == PKTSIZE){
			 process_packet((FAST_input_databuf_t *)db,packet);
		}
		pthread_testcancel();
	}// Main loop
	return THREAD_OK;
}

static hashpipe_thread_desc_t FAST_net_thread = {
	name: "FAST_net_thread",
	skey: "NETSTAT",
	init: init,
	run:  run,
	ibuf_desc: {NULL},
	obuf_desc: {FAST_input_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
	register_hashpipe_thread(&FAST_net_thread);
}
