/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Simon Goldschmidt
 *
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#include "lpc17xx_emac.h"

/* Select NO_SYS based on OS or Non OS configuration */
#define LWIP_NETCONN                    1
#define LWIP_SOCKET                     0

#define LWIP_IPV6						0
#define LWIP_SO_RCVTIMEO                1		/* Use receive timeout to netconn. */

/* Freertos/lwip/PHY config. */
#define LOCK_RX_THREAD					0		/* To protect RX Processing. */
#define LPC_TX_PBUF_BOUNCE_EN			0

#define MAC_NO_FILTER					0
#if !MAC_NO_FILTER
#define IP_SOF_BROADCAST                1
#define IP_SOF_BROADCAST_RECV           1
#endif

/* Some optimizations... */
#define IP_REASSEMBLY                   0
#define IP_FRAG                         0
#define TCP_MSS                         256

/* Need for memory protection */
#define SYS_LIGHTWEIGHT_PROT            (NO_SYS == 0)

/* 32-bit alignment */
#define MEM_ALIGNMENT                   4

/* DHCP is ok, UDP is required with DHCP */
#define LWIP_DHCP                       1

#define LWIP_IGMP						1	/* Enable Multicast features. */
#if LWIP_IGMP
#define LWIP_RAND()						rand()
#endif

#define LWIP_NETIF_HOSTNAME             1	/* Hostname can be used */

#define LWIP_DEBUG						0
#define LWIP_NOASSERT					0

/* This define is custom for the LPC EMAC driver. Enabled it to get debug messages for the driver. */
#define EMAC_DEBUG                    	LWIP_DBG_OFF

#define LWIP_STATS                      0
#if LWIP_STATS
#define LWIP_STATS_DISPLAY              1
#define LINK_STATS                      1
#define ETHARP_STATS                    0
#define IP_STATS                        0
#define IPFRAG_STATS                    0
#define ICMP_STATS                      0
#define IGMP_STATS                      0
#define UDP_STATS                       1
#define TCP_STATS                       0
#define MEM_STATS                       0
#define MEMP_STATS                      0
#define SYS_STATS                       0
#define IP6_STATS                       0
#define ICMP6_STATS                     0
#define IP6_FRAG_STATS                  0
#define MLD6_STATS                      0
#define ND6_STATS                       0
#define MIB2_STATS                      0
#endif

#define DEFAULT_THREAD_PRIO            	(tskIDLE_PRIORITY + 2)
#define DEFAULT_THREAD_STACKSIZE       	(configMINIMAL_STACK_SIZE * 4)

/* The number of application tasks communicating with the tcpip_thread + the number of input packets queued for receiving. */
#define DEFAULT_ACCEPTMBOX_SIZE        	4
#define DEFAULT_TCP_RECVMBOX_SIZE      	4
#define DEFAULT_UDP_RECVMBOX_SIZE      	4
#define TCPIP_MBOX_SIZE                	(DEFAULT_ACCEPTMBOX_SIZE + DEFAULT_TCP_RECVMBOX_SIZE + DEFAULT_UDP_RECVMBOX_SIZE)

/* TCPIP thread must run at higher priority than MAC threads! */
#define TCPIP_THREAD_PRIO              	(DEFAULT_THREAD_PRIO + 3)
#define TCPIP_THREAD_STACKSIZE         	(DEFAULT_THREAD_STACKSIZE * 3)

#define LWIP_CHECKSUM_ON_COPY           1

/* Config for Pre Allocation POOL.*/
//#define PBUF_POOL_SIZE					(2 * EMAC_NUM_RX_FRAG)
//#define MEM_SIZE						(PBUF_POOL_SIZE * EMAC_ETH_MAX_FLEN)

/* Configurando malloc/free para o LWIP usando o FreeRTOS para gerenciar o acesso a memória. */
#define MEM_LIBC_MALLOC                 1
#define MEMP_MEM_MALLOC                 1

#include <string.h>
#include "FreeRTOS.h"

#define mem_free 	vPortFree
#define mem_malloc  pvPortMalloc

static inline void *pvPortCalloc(size_t nmemb, size_t size)
{
	void *x = mem_malloc(nmemb * size);
	if (x != NULL)
		memset(x, 0, nmemb * size);
	return x;
}

#define mem_calloc pvPortCalloc

#endif /* __LWIPOPTS_H__ */
