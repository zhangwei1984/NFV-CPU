/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>

#include "common.h"

#define MAX_MSG 128

/* Number of packets to attempt to read from queue */
#define PKT_READ_SIZE  ((uint16_t)32)

/* our client id number - tells us which rx queue to read, and NIC TX
 * queue to write to. */
static uint8_t client_id = 0;

struct mbuf_queue {
#define MBQ_CAPACITY 32
	struct rte_mbuf *bufs[MBQ_CAPACITY];
	uint16_t top;
};

/* maps input ports to output ports for packets */
static uint8_t output_ports[RTE_MAX_ETHPORTS];

/* buffers up a set of packet that are ready to send */
static struct mbuf_queue output_bufs[RTE_MAX_ETHPORTS];

/* shared data from server. We update statistics here */
static volatile struct tx_stats *tx_stats;

static volatile uint64_t pkt_count = 0;
static volatile uint64_t msg_count = 0;
static volatile uint64_t bad_wakeup_count = 0;
static volatile uint64_t bad_batch_count = 0;

#ifdef CLIENT_STAT
#define TIMER_SECOND 2670000000ULL
#define SLEEP_TIME 1
static uint64_t  timer_period = SLEEP_TIME * TIMER_SECOND; 

static int
stat_print(__attribute__((unused)) void *dummy) 
{
	uint64_t prev_tsc, cur_tsc;
	prev_tsc = rte_rdtsc();
	uint64_t prev_pkt_count = pkt_count;
	uint64_t prev_msg_count = msg_count;
	uint64_t prev_bad_wakeup_count = bad_wakeup_count;
	uint64_t prev_bad_batch_count = bad_batch_count;
	double period_time;
	uint64_t pkt_rate, msg_rate, bad_wakeup_rate, bad_batch_rate;
	

	while (1) {
		sleep(SLEEP_TIME);	

		cur_tsc = rte_rdtsc();
		if (cur_tsc - prev_tsc >= timer_period){	
			period_time = (cur_tsc - prev_tsc) / (double)TIMER_SECOND; 
			pkt_rate = (pkt_count - prev_pkt_count) / period_time;
			msg_rate = (msg_count - prev_msg_count) / period_time;
			bad_wakeup_rate = (bad_wakeup_count - prev_bad_wakeup_count) / period_time;
			bad_batch_rate = (bad_batch_count - prev_bad_batch_count) / period_time;
			printf("pkt_rate=%"PRIu64", msg_rate=%"PRIu64", bad_wakeup_rate=%"PRIu64", bad_batch_rate=%"PRIu64", period_time=%f\n", 
				pkt_rate, msg_rate, bad_wakeup_rate, bad_batch_rate, period_time);
			prev_tsc = cur_tsc;	
			prev_pkt_count = pkt_count;
			prev_msg_count = msg_count;
			prev_bad_wakeup_count = bad_wakeup_count;
			prev_bad_batch_count = bad_batch_count;

		}
	}
	return 0;

}
#endif

/*
 * print a usage message
 */
static void
usage(const char *progname)
{
	printf("Usage: %s [EAL args] -- -n <client_id>\n\n", progname);
}

/*
 * Convert the client id number from a string to an int.
 */
static int
parse_client_num(const char *client)
{
	char *end = NULL;
	unsigned long temp;

	if (client == NULL || *client == '\0')
		return -1;

	temp = strtoul(client, &end, 10);
	if (end == NULL || *end != '\0')
		return -1;

	client_id = (uint8_t)temp;
	return 0;
}

/*
 * Parse the application arguments to the client app.
 */
static int
parse_app_args(int argc, char *argv[])
{
	int option_index, opt;
	char **argvopt = argv;
	const char *progname = NULL;
	static struct option lgopts[] = { /* no long options */
		{NULL, 0, 0, 0 }
	};
	progname = argv[0];

	while ((opt = getopt_long(argc, argvopt, "n:", lgopts,
		&option_index)) != EOF){
		switch (opt){
			case 'n':
				if (parse_client_num(optarg) != 0){
					usage(progname);
					return -1;
				}
				break;
			default:
				usage(progname);
				return -1;
		}
	}
	return 0;
}

/*
 * set up output ports so that all traffic on port gets sent out
 * its paired port. Index using actual port numbers since that is
 * what comes in the mbuf structure.
 */
static void configure_output_ports(const struct port_info *ports)
{
	int i;
	if (ports->num_ports > RTE_MAX_ETHPORTS)
		rte_exit(EXIT_FAILURE, "Too many ethernet ports. RTE_MAX_ETHPORTS = %u\n",
				(unsigned)RTE_MAX_ETHPORTS);
	for (i = 0; i < ports->num_ports; i+=1){
		uint8_t p = ports->id[i];
		output_ports[p] = p;
	}
}

static inline void
drop_packet(struct rte_mbuf *pkt)
{
	const uint8_t in_port = pkt->port;
	const uint8_t out_port = output_ports[in_port];	

	rte_pktmbuf_free(pkt);
	tx_stats->tx_drop[out_port] += 1;
} 

static inline void
drop_packets(uint8_t port)
{
        struct mbuf_queue *mbq = &output_bufs[port];
	int i;

        if (unlikely(mbq->top == 0))
                return;

	for (i = 0; i < mbq->top; i++) {
		rte_pktmbuf_free(mbq->bufs[i]);
		tx_stats->tx_drop[port] += mbq->top;

	}
        mbq->top = 0;
}


static inline void
send_packets(uint8_t port)
{
	uint16_t i, sent;
	struct mbuf_queue *mbq = &output_bufs[port];

	if (unlikely(mbq->top == 0))
		return;

	sent = rte_eth_tx_burst(port, client_id, mbq->bufs, mbq->top);
	if (unlikely(sent < mbq->top)){
		for (i = sent; i < mbq->top; i++)
			rte_pktmbuf_free(mbq->bufs[i]);
		tx_stats->tx_drop[port] += (mbq->top - sent);
	}
	tx_stats->tx[port] += sent;
	mbq->top = 0;
}

/*
 * Enqueue a packet to be sent on a particular port, but
 * don't send it yet. Only when the buffer is full.
 */
static inline void
enqueue_packet(struct rte_mbuf *buf, uint8_t port)
{
	struct mbuf_queue *mbq = &output_bufs[port];
	mbq->bufs[mbq->top++] = buf;

	if (mbq->top == MBQ_CAPACITY)
		send_packets(port);
}

/*
 * This function performs routing of packets
 * Just sends each input packet out an output port based solely on the input
 * port it arrived on.
 */
static void
handle_packet(struct rte_mbuf *buf)
{
	const uint8_t in_port = buf->port;
	const uint8_t out_port = output_ports[in_port];

	enqueue_packet(buf, out_port);
}

/*
 * Application main function - loops through
 * receiving and processing packets. Never returns
 */
int
main(int argc, char *argv[])
{
	const struct rte_memzone *mz;
	struct rte_ring *rx_ring;
	struct rte_mempool *mp;
	struct port_info *ports;
	int need_flush = 0; /* indicates whether we have unsent packets */
	int retval;
	void *pkts[PKT_READ_SIZE];
	
	#ifdef INTERRUPT_FIFO
	const char *fifo_name;
	FILE *fifo_fp;
	char msg[MAX_MSG];
	#endif

	#ifdef INTERRUPT_SEM
	sem_t *mutex;
	const char *sem_name;
	#endif

	#if defined(INTERRUPT_FIFO) || defined(INTERRUPT_SEM)
	#ifdef DPDK_FLAG
	int *irq_flag; 
	#else
	int shmid;
    	key_t key;
    	char *shm;
	int *flag_p;	
	#endif
	#endif

	if ((retval = rte_eal_init(argc, argv)) < 0)
		return -1;
	argc -= retval;
	argv += retval;

	if (parse_app_args(argc, argv) < 0)
		rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

	if (rte_eth_dev_count() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	rx_ring = rte_ring_lookup(get_rx_queue_name(client_id));
	if (rx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");

	mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

	mz = rte_memzone_lookup(MZ_PORT_INFO);
	if (mz == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get port info structure\n");
	ports = mz->addr;
	tx_stats = &(ports->tx_stats[client_id]);

	configure_output_ports(ports);

	#ifdef INTERRUPT_FIFO	
	//FIFO pipe for message comminucation between client and server
	fifo_name = get_fifo_name(client_id);
	fifo_fp = fopen(fifo_name, "r");
	if (fifo_fp == NULL) {
		fprintf(stderr, "can not open FIFO for client %d\n", client_id);
		exit(1);
	}
	#endif

	#ifdef INTERRUPT_SEM
	sem_name = get_sem_name(client_id);
	fprintf(stderr, "sem_name=%s for client %d\n", sem_name, client_id);
	mutex = sem_open(sem_name, 0, 0666, 0);
	if (mutex == SEM_FAILED) {
		perror("Unable to execute semaphore");
		fprintf(stderr, "unable to execute semphore for client %d\n", client_id);
		sem_close(mutex);
		exit(1);
	}
	#endif

	#if defined(INTERRUPT_FIFO) || defined(INTERRUPT_SEM)
	#ifdef DPDK_FLAG
	irq_flag = (int *)rte_ring_lookup(get_irq_flag_name(client_id));
        if (irq_flag == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get irq flag - is server process running?\n");	
	#else
	key = get_rx_shmkey(client_id);
	if ((shmid = shmget(key, SHMSZ, 0666)) < 0) {
        	perror("shmget");
		fprintf(stderr, "unable to Locate the segment for client %d\n", client_id);
        	exit(1);
   	}

	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        	fprintf(stderr, "can not attach the shared segment to the client space for client %d\n", client_id);
                exit(1);
        }

	flag_p = (int *)shm;
	#endif
	#endif


	RTE_LOG(INFO, APP, "Finished Process Init.\n");

	#ifdef CLIENT_STAT
	unsigned cur_lcore = rte_lcore_id();
	unsigned next_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
	printf("cur_lcore=%u, next_lcore=%u\n", cur_lcore, next_lcore);
	rte_eal_remote_launch(stat_print, NULL, next_lcore);
	#endif

	printf("\nClient process %d handling packets\n", client_id);
	printf("[Press Ctrl-C to quit ...]\n");

	for (;;) {
		uint16_t i;
		uint8_t port;
		
		#if defined(INTERRUPT_FIFO) || defined(INTERRUPT_SEM)
		#ifdef DPDK_FLAG
		*irq_flag = 1;
		#else
		*flag_p = 1;
		#endif
		#endif

		#ifdef INTERRUPT_FIFO
		char* ret;
		memset(msg, 0, sizeof(msg));
		ret = fgets(msg, MAX_MSG, fifo_fp);
	
		if (ret == NULL) {
			exit(1);
		}
	
		msg_count++;

		#ifdef DEBUG
		fprintf(stderr, "receive message: %s", msg);
		#endif

		#if 0
		if (strncmp(msg, "wakeup", 6) != 0) {
			continue;
		}
		#endif

		#endif

		#ifdef INTERRUPT_SEM
		sem_wait(mutex);
		msg_count++;
		#ifdef DEBUG
		fprintf(stderr, "client is woken up%d\n", client_id);	
		#endif
		#endif

		#if defined(INTERRUPT_FIFO) || defined(INTERRUPT_SEM)
		#ifdef DPDK_FLAG
                *irq_flag = 0;
                #else
                *flag_p = 0;
                #endif
		#endif

		while (1){
			uint16_t rx_pkts = PKT_READ_SIZE;
		/* try dequeuing max possible packets first, if that fails, get the
		 * most we can. Loop body should only execute once, maximum */
			while (rx_pkts > 0 &&
					unlikely(rte_ring_dequeue_bulk(rx_ring, pkts, rx_pkts) != 0))
				rx_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring), PKT_READ_SIZE);
		
			pkt_count += rx_pkts;
			if (rx_pkts < PKT_READ_SIZE) {
				bad_batch_count++;
			}
				
			if (unlikely(rx_pkts == 0)){
				if (need_flush)
					for (port = 0; port < ports->num_ports; port++){
						if (client_id % 2 == 0) {	
							send_packets(ports->id[port]);
						}
						else {
							drop_packets(ports->id[port]);
						}
					}
				else {
					bad_wakeup_count++;
				}
				need_flush = 0;
				break;
			}

			for (i = 0; i < rx_pkts; i++) {
				if (client_id % 2 == 0) {
					handle_packet(pkts[i]);
				}
				else {
					drop_packet(pkts[i]);
				}
			}

			need_flush = 1;
		}
	}
}
