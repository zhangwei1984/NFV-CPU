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

#ifndef _COMMON_H_
#define _COMMON_H_

#include "shared.h"

#define MAX_CLIENTS             16

/*
 * Shared port info, including statistics information for display by server.
 * Structure will be put in a memzone.
 * - All port id values share one cache line as this data will be read-only
 * during operation.
 * - All rx statistic values share cache lines, as this data is written only
 * by the server process. (rare reads by stats display)
 * - The tx statistics have values for all ports per cache line, but the stats
 * themselves are written by the clients, so we have a distinct set, on different
 * cache lines for each client to use.
 */
struct rx_stats{
	uint64_t rx[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

struct tx_stats{
	uint64_t tx[RTE_MAX_ETHPORTS];
	uint64_t tx_drop[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

struct port_info {
	uint8_t num_ports;
	uint8_t id[RTE_MAX_ETHPORTS];
	volatile struct rx_stats rx_stats;
	volatile struct tx_stats tx_stats[MAX_CLIENTS];
};

/* define common names for structures shared between server and client */
#define MP_CLIENT_RXQ_NAME "MProc_Client_%u_RX"
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define MZ_PORT_INFO "MProc_port_info"
//add by wei
#ifdef INTERRUPT_FIFO
#define MP_CLIENT_FIFO_NAME "MProc_Client_%u_FIFO"
#endif

#ifdef INTERRUPT_SEM
#define MP_CLIENT_SEM_NAME "MProc_Client_%u_SEM"
#endif

#define SHMSZ 4
#define KEY_PREFIX 123

#define MP_CLIENT_IRQ_FLAG_NAME "MProc_Client_%u_IRQ_FLAG"

/*
 * Given the irq flag name template above, get the flag name
 */
static inline const char *
get_irq_flag_name(unsigned id)
{
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_CLIENT_IRQ_FLAG_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_IRQ_FLAG_NAME, id);
        return buffer;
}


/*
 * Given the rx queue name template above, get the key of the shared memory
 */
static inline key_t
get_rx_shmkey(unsigned id)
{
        return KEY_PREFIX * 10 + id;
}


/*
 * Given the rx queue name template above, get the queue name
 */
static inline const char *
get_rx_queue_name(unsigned id)
{
	/* buffer for return value. Size calculated by %u being replaced
	 * by maximum 3 digits (plus an extra byte for safety) */
	static char buffer[sizeof(MP_CLIENT_RXQ_NAME) + 2];

	snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_RXQ_NAME, id);
	return buffer;
}

#ifdef INTERRUPT_FIFO
/*
 * Given the fifo name template above, get the fifo name
 */
static inline const char *
get_fifo_name(unsigned id)
{
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_CLIENT_FIFO_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_FIFO_NAME, id);
        return buffer;
}
#endif

#ifdef INTERRUPT_SEM
/*
 * Given the sem name template above, get the sem name
 */
static inline const char *
get_sem_name(unsigned id)
{
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_CLIENT_SEM_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_SEM_NAME, id);
        return buffer;
}
#endif


#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

#endif
