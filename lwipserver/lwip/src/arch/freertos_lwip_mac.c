/*
 * @brief LPC17xx/40xx LWIP EMAC driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "LPC17xx.h"
#include "FreeRTOS.h"

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/igmp.h"
#include "netif/etharp.h"
#include "lwipopts.h"

#include "phy_smsc87x0.h"
#include "freertos_lwip_mac.h"

/** @ingroup NET_LWIP_LPC17XX40XX_EMAC_DRIVER
 * @{
 */

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/* LPC EMAC driver data structure */
typedef struct
{
	/* prxs must be 8 byte aligned! */
	RX_Stat prxs[EMAC_NUM_RX_FRAG];				/**< Pointer to RX status */
	RX_Desc prxd[EMAC_NUM_RX_FRAG];				/**< Pointer to RX descriptor list */
	TX_Stat ptxs[EMAC_NUM_TX_FRAG];				/**< Pointer to TX status */
	TX_Desc ptxd[EMAC_NUM_TX_FRAG];				/**< Pointer to TX descriptor list */

	struct netif *pnetif;						/**< Reference back to LWIP parent netif */
	struct pbuf *rxb[EMAC_NUM_RX_FRAG];			/**< RX pbuf pointer list, zero-copy mode */
	struct pbuf *txb[EMAC_NUM_TX_FRAG];			/**< TX pbuf pointer list, zero-copy mode */

	u8_t rx_free_descs;							/**< Count of free RX descriptors */
	u8_t rx_fill_desc_index;					/**< RX descriptor next available index */
//	u8_t rx_queued_frames;						/**< Count of queued RX Frames. */
	u8_t lpc_last_tx_idx;						/**< TX last descriptor index, zero-copy mode */

	sys_sem_t rx_sem;							/**< RX receive thread wakeup semaphore */
	sys_sem_t tx_clean_sem;						/**< TX cleanup thread wakeup semaphore */
	sys_sem_t xtx_count_sem;					/**< TX free buffer counting semaphore */
	sys_mutex_t tx_lock_mutex;					/**< TX critical section mutex */
	sys_mutex_t rx_lock_mutex;					/**< RX critical section mutex */
} lpc_enetdata_t;

/** \brief  LPC EMAC driver work data. */
ALIGNED(8) static lpc_enetdata_t lpc_enetdata;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/
/* MII Mgmt Configuration register - Clock divider setting */
const u8_t ethclkdiv[] = {4, 6, 8, 10, 14, 20, 28, 36, 40, 44, 48, 52, 56, 60, 64};

/*****************************************************************************
 * Private functions
 ****************************************************************************/
extern void msDelay(uint32_t ms);
static u8_t lpc_tx_ready(struct netif *netif);
static void lpc_tx_reclaim_st(lpc_enetdata_t *lpc_enetif, u8_t cidx);
static void lpc_rxqueue_pbuf(lpc_enetdata_t *lpc_enetif, struct pbuf *p);
static err_t lpc_tx_setup(lpc_enetdata_t *lpc_enetif);
static bool IsRxEmpty(void);

/* Queues a pbuf into the RX descriptor list */
static void lpc_rxqueue_pbuf(lpc_enetdata_t *lpc_enetif, struct pbuf *p)
{
	u8_t idx = 0;

	/* Get next free descriptor index */
	idx = lpc_enetif->rx_fill_desc_index;

	/* Setup descriptor and clear statuses */
	lpc_enetif->prxd[idx].Ctrl = EMAC_RCTRL_INT | ((u32_t) EMAC_RCTRL_SIZE((p->len - 1)));
	lpc_enetif->prxd[idx].Packet = (u32_t) p->payload;
	lpc_enetif->prxs[idx].Info = 0xffffffff;
	lpc_enetif->prxs[idx].HashCRC = 0xffffffff;

	/* Save pbuf pointer for push to network layer later */
	lpc_enetif->rxb[idx] = p;

	/* Wrap at end of descriptor list */
	idx++;
	if (idx >= EMAC_NUM_RX_FRAG)
	{
		idx = 0;
	}

	/* Queue descriptor(s) */
	lpc_enetif->rx_free_descs -= 1;
	lpc_enetif->rx_fill_desc_index = idx;

	LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE, ("lpc_rxqueue_pbuf: pbuf packet queued: %p (free desc=%d)\n", p, lpc_enetif->rx_free_descs));
}

/* Sets up the TX descriptor ring buffers */
static err_t lpc_tx_setup(lpc_enetdata_t *lpc_enetif)
{
	u8_t idx = 0;

	/* Build TX descriptors for local buffers */
	for (idx = 0; idx < EMAC_NUM_TX_FRAG; idx++)
	{
		lpc_enetif->ptxd[idx].Ctrl = 0;
		lpc_enetif->ptxs[idx].Info = 0xffffffff;
	}

	/* Setup pointers to TX structures */
	LPC_EMAC->TxDescriptor = (u32_t)lpc_enetif->ptxd;
	LPC_EMAC->TxDescriptorNumber = (EMAC_NUM_TX_FRAG - 1);
	LPC_EMAC->TxStatus = (u32_t)lpc_enetif->ptxs;
	LPC_EMAC->TxProduceIndex = 0;

	lpc_enetif->lpc_last_tx_idx = 0;

	return (ERR_OK);
}

/* Sets up the RX descriptor ring buffers. */
static err_t lpc_rx_setup(lpc_enetdata_t *lpc_enetif)
{
	/* Setup pointers to RX structures */
	LPC_EMAC->RxDescriptor = (u32_t) lpc_enetif->prxd;
	LPC_EMAC->RxDescriptorNumber = (EMAC_NUM_RX_FRAG - 1);
	LPC_EMAC->RxStatus = (u32_t) lpc_enetif->prxs;
	LPC_EMAC->RxConsumeIndex = 0;

	lpc_enetif->rx_free_descs = EMAC_NUM_RX_FRAG;
	lpc_enetif->rx_fill_desc_index = 0;

	/* Build RX buffer and descriptors */
	lpc_rx_queue(lpc_enetif->pnetif);

	return (ERR_OK);
}

/***********************************************************************
 * @brief 		Write value to PHY device
 * @param[in] 	PhyReg: PHY Register address
 * @param[in] 	Value:  Value to write
 * @return 		0 - if success
 * 				1 - if fail
 ***********************************************************************/
s8_t writephy(u32_t phyreg, u8_t address, u16_t value)
{
	/* Write a data 'Value' to PHY register 'PhyReg'. */
	u32_t tout;

	/* Write value at PHY address and register */
	LPC_EMAC->MCMD = 0;
	LPC_EMAC->MADR = (((address & 0x1f) << 8) | (phyreg & 0x1f));
	LPC_EMAC->MWTD = value;

	/* Wait until operation completed */
	tout = 0;
	for (tout = 0; tout < EMAC_MII_WR_TOUT; tout++)
	{
		if ((LPC_EMAC->MIND & EMAC_MIND_BUSY) == 0)
		{
			return (0);
		}
	}
	// Time out!
	return (-1);
}

/*********************************************************************//**
 * @brief 		Read value from PHY device
 * @param[in] 	PhyReg: PHY Register address
 * @return 		0 - if success
 * 				1 - if fail
 ***********************************************************************/
s32_t readphy(u32_t phyreg, u8_t address)
{
	/* Read a PHY register 'PhyReg'. */
	u32_t tout;

	LPC_EMAC->MADR = (((address & 0x1f) << 8) | (phyreg & 0x1f));
	LPC_EMAC->MCMD = EMAC_MCMD_READ;

	/* Wait until operation completed */
	tout = 0;
	for (tout = 0; tout < EMAC_MII_RD_TOUT; tout++)
	{
		if ((LPC_EMAC->MIND & EMAC_MIND_BUSY) == 0)
		{
			LPC_EMAC->MCMD = 0;
			return (LPC_EMAC->MRDD);
		}
	}
	// Time out!
	return (-1);
}

/* Allocates a pbuf and returns the data from the incoming packet */
static struct pbuf *lpc_low_level_input(struct netif *netif)
{
	lpc_enetdata_t *lpc_enetif = netif->state;
	struct pbuf *p = NULL;
	u8_t idx = 0;
	u16_t length = 0;

#if LOCK_RX_THREAD
	/* Get exclusive access */
	sys_mutex_lock(&lpc_enetif->rx_lock_mutex);
#endif

	/* Monitor RX overrun or RX error status. This should never happen unless
	   (possibly) the internal bus is behing held up by something. Unless your
	   system is running at a very low clock speed or there are possibilities
	   that the internal buses may be held up for a long time, this can
	   probably safely be removed. */
	if(LPC_EMAC->IntStatus & (EMAC_INT_RX_OVERRUN | EMAC_INT_RX_ERR))
	{
		LINK_STATS_INC(link.err);
		LINK_STATS_INC(link.drop);

		/* Temporarily disable RX */
		LPC_EMAC->Command &= ~EMAC_CR_RX_EN;
		LPC_EMAC->MAC1 &= ~EMAC_MAC1_REC_EN;

		/* Reset the RX side */
		LPC_EMAC->MAC1 |= EMAC_MAC1_RES_RX;
		LPC_EMAC->IntClear = (EMAC_INT_RX_OVERRUN | EMAC_INT_RX_ERR);

		/* De-allocate all queued RX pbufs */
		for (idx = 0; idx < EMAC_NUM_RX_FRAG; idx++)
		{
			if (lpc_enetif->rxb[idx] != NULL)
			{
				pbuf_free(lpc_enetif->rxb[idx]);
				lpc_enetif->rxb[idx] = NULL;
			}
		}

		/* Start RX side again */
		lpc_rx_setup(lpc_enetif);

		/* Re-enable RX */
		LPC_EMAC->Command |= EMAC_CR_RX_EN;
		LPC_EMAC->MAC1 |= EMAC_MAC1_REC_EN;
	}
	else
	{
		idx = LPC_EMAC->RxConsumeIndex;

		if (!IsRxEmpty())
		{
			/* Handle errors */
			if (lpc_enetif->prxs[idx].Info & (EMAC_RINFO_CRC_ERR | EMAC_RINFO_SYM_ERR | EMAC_RINFO_ALIGN_ERR | EMAC_RINFO_LEN_ERR))
			{
#if LINK_STATS
				if (lpc_enetif->prxs[idx].Info & (EMAC_RINFO_CRC_ERR | EMAC_RINFO_SYM_ERR | EMAC_RINFO_ALIGN_ERR))
				{
					LINK_STATS_INC(link.chkerr);
				}

				if (lpc_enetif->prxs[idx].Info & EMAC_RINFO_LEN_ERR)
				{
					LINK_STATS_INC(link.lenerr);
				}
#endif
				/* Drop the frame */
				LINK_STATS_INC(link.drop);

				/* Re-queue the pbuf for receive */
				lpc_enetif->rx_free_descs++;

				p = lpc_enetif->rxb[idx];

				lpc_enetif->rxb[idx] = NULL;

				lpc_rxqueue_pbuf(lpc_enetif, p);

				LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE, ("lpc_low_level_input: Packet dropped with errors (0x%x)\n", lpc_enetif->prxs[idx].Info));

				p = NULL;
			}
			else
			{
				/* A packet is waiting, get length */
				length = ((((lpc_enetif->prxs[idx].Info) & EMAC_RINFO_SIZE) + 1) - 4);	/* Remove FCS */

				if(length > 0)
				{
					/* Zero-copy */
					p = lpc_enetif->rxb[idx];
					p->len = length;

					/* Free pbuf from descriptor */
					lpc_enetif->rxb[idx] = NULL;
					lpc_enetif->rx_free_descs++;

					/* Queue new buffer(s) */
					if (lpc_rx_queue(lpc_enetif->pnetif) == 0)
					{
						lpc_rxqueue_pbuf(lpc_enetif, p);	/* Re-queue the pbuf for receive */

						/* Drop the frame */
						LINK_STATS_INC(link.drop);
						LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE, ("lpc_low_level_input: Packet dropped since it could not allocate Rx Buffer\n"));
						p = NULL;
					}
					else
					{
						LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE, ("lpc_low_level_input: Packet received: %p, size %d (index=%d)\n", p, length, idx));
						p->tot_len = length;		/* Save size */
						LINK_STATS_INC(link.recv);
					}
				}
			}

			/* Update Consume index */
			u8_t idxr = LPC_EMAC->RxConsumeIndex;
			idxr++;

			if(idxr > LPC_EMAC->RxDescriptorNumber)
			{
				idxr = 0;
			}
			LPC_EMAC->RxConsumeIndex = idxr;
		}
	}

#if LOCK_RX_THREAD
	sys_mutex_unlock(&lpc_enetif->rx_lock_mutex);
#endif

	return (p);
}

/* Determine if the passed address is usable for the ethernet DMA controller */
static u8_t lpc_packet_addr_notsafe(void *addr)
{
	/* Check for legal address ranges for LPC176x */
	if ((((u32_t) addr >= 0x10000000) && ((u32_t) addr < 0x10008000)) /* 32kB local SRAM */
		|| (((u32_t) addr >= 0x1FFF0000) && ((u32_t) addr < 0x1FFF2000)) /* 8kB ROM */
		|| (((u32_t) addr >= 0x2007C000) && ((u32_t) addr < 0x20084000)) /* 32kB AHB SRAM */
		) {
		return 0;
	}
	return 1;
}

/* Free TX buffers that are complete */
static void lpc_tx_reclaim_st(lpc_enetdata_t *lpc_enetif, u8_t cidx)
{
#if NO_SYS == 0
	sys_mutex_lock(&lpc_enetif->tx_lock_mutex);	/* Get exclusive access */
#endif

	while (cidx != lpc_enetif->lpc_last_tx_idx)
	{
		if (lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx] != NULL)
		{
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE, ("lpc_tx_reclaim_st: Freeing packet %p (index %d)\n", lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx], lpc_enetif->lpc_last_tx_idx));
			pbuf_free(lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx]);
			lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx] = NULL;
		}

#if NO_SYS == 0
		sys_sem_signal(&lpc_enetif->xtx_count_sem);
#endif

		lpc_enetif->lpc_last_tx_idx++;
		if (lpc_enetif->lpc_last_tx_idx >= EMAC_NUM_TX_FRAG)
		{
			lpc_enetif->lpc_last_tx_idx = 0;
		}
	}
#if NO_SYS == 0
	sys_mutex_unlock(&lpc_enetif->tx_lock_mutex);	/* Restore access */
#endif
}

/* Low level output of a packet. Never call this from an interrupt context,
 * as it may block until TX descriptors become available. */
static err_t lpc_low_level_output(struct netif *netif, struct pbuf *p)
{
#if LPC_TX_PBUF_BOUNCE_EN == 1
	u8_t *dst;
	struct pbuf *np;
#endif
	lpc_enetdata_t *lpc_enetif = netif->state;
	struct pbuf *q = NULL;
	u8_t idx = 0, dn = 0;
	u32_t notdmasafe = 0;

	/* Zero-copy TX buffers may be fragmented across mutliple payload
	   chains. Determine the number of descriptors needed for the
	   transfer. The pbuf chaining can be a mess! */
	dn = pbuf_clen(p);

	/* Test to make sure packet addresses are DMA safe. A DMA safe
	   address is once that uses external memory or peripheral RAM.
	   IRAM and FLASH are not safe! */
	for (q = p; q != NULL; q = q->next)
	{
		notdmasafe += lpc_packet_addr_notsafe(q->payload);
	}

#if LPC_TX_PBUF_BOUNCE_EN == 1
	/* If the pbuf is not DMA safe, a new bounce buffer (pbuf) will be
	   created that will be used instead. This requires an copy from the
	   non-safe DMA region to the new pbuf */
	if (notdmasafe)
	{
		/* Allocate a pbuf in DMA memory */
		np = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
		if (np == NULL)
		{
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE, ("lpc_low_level_output: could not allocate TX pbuf\n"));
			return ERR_MEM;
		}

		/* This buffer better be contiguous! */
		LWIP_ASSERT("lpc_low_level_output: New transmit pbuf is chained", (pbuf_clen(np) == 1));

		/* Copy to DMA safe pbuf */
		dst = (u8_t *) np->payload;
		for (q = p; q != NULL; q = q->next)
		{
			/* Copy the buffer to the descriptor's buffer */
			MEMCPY(dst, (u8_t *) q->payload, q->len);
			dst += q->len;
		}
		np->len = p->tot_len;

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE, ("lpc_low_level_output: Switched to DMA safe buffer, old=%p, new=%p\n", q, np));

		/* use the new buffer for descrptor queueing. The original pbuf will
		   be de-allocated outsuide this driver. */
		p = np;
		dn = 1;
	}
#else
	if (notdmasafe)
	{
		LWIP_ASSERT("lpc_low_level_output: Not a DMA safe pbuf", (notdmasafe == 0));
	}
#endif

	/* Wait until enough descriptors are available for the transfer. */
	/* THIS WILL BLOCK UNTIL THERE ARE ENOUGH DESCRIPTORS AVAILABLE */
	while (dn > lpc_tx_ready(netif))
	{
#if NO_SYS == 0
		sys_arch_sem_wait(lpc_enetif->xtx_count_sem, 0);
#endif
	}

	/* Get free TX buffer index */
	idx = LPC_EMAC->TxProduceIndex;

#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_enetif->tx_lock_mutex);
#endif

	/* Prevent LWIP from de-allocating this pbuf. The driver will free it once it's been transmitted. */
	if (!notdmasafe)
	{
		pbuf_ref(p);
	}

	/* Setup transfers */
	q = p;
	while (dn > 0)
	{
		dn--;

		/* Only save pointer to free on last descriptor */
		if (dn == 0)
		{
			/* Save size of packet and signal it's ready */
			lpc_enetif->ptxd[idx].Ctrl = ((q->len - 1) & EMAC_TCTRL_SIZE) | EMAC_TCTRL_INT | EMAC_TCTRL_LAST;
			lpc_enetif->txb[idx] = p;
		}
		else
		{
			/* Save size of packet, descriptor is not last */
			lpc_enetif->ptxd[idx].Ctrl = ((q->len - 1) & EMAC_TCTRL_SIZE) | EMAC_TCTRL_INT;
			lpc_enetif->txb[idx] = NULL;
		}

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE, ("lpc_low_level_output: pbuf packet(%p) sent, chain#=%d,"" size = %d (index=%d)\n", q->payload, dn, q->len, idx));

		lpc_enetif->ptxd[idx].Packet = (u32_t) q->payload;

		q = q->next;

		idx = LPC_EMAC->TxProduceIndex;
		idx++;
		if(idx > LPC_EMAC->TxDescriptorNumber)
		{
			idx = 0;
		}
		LPC_EMAC->TxProduceIndex = idx;
	}

	LINK_STATS_INC(link.xmit);

#if NO_SYS == 0
	/* Give Up exclusive access */
	sys_mutex_unlock(&lpc_enetif->tx_lock_mutex);		/* Restore access */
#endif

	return (ERR_OK);
}

static bool IsRxEmpty(void)
{
	u8_t tem;

	tem = LPC_EMAC->RxProduceIndex;

	return ((LPC_EMAC->RxConsumeIndex == tem) ? true : false);
}

#if NO_SYS == 0
/* Packet reception task for FreeRTOS */
static void vPacketReceiveTask(void *pvParameters)
{
	lpc_enetdata_t *lpc_enetif = pvParameters;
//	UBaseType_t uxHighWaterMark;

	/* Inspect our own high water mark on entering the task. */
//	uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

	while (1)
	{
		/* Calling the function will have used some stack space, we would
		therefore now expect uxTaskGetStackHighWaterMark() to return a
		value lower than when it was called on entering the task. */
//		uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
//
//		printf("\nRx Heap: %d", uxHighWaterMark);

		/* Wait for receive task to wakeup */
		sys_arch_sem_wait(&lpc_enetif->rx_sem, 0);

		/* Process packets until all empty */
		while (!IsRxEmpty())
		{
			lpc_enetif_input(lpc_enetif->pnetif);
		}
	}
}

/* Transmit cleanup task for FreeRTOS */
static void vTransmitCleanupTask(void *pvParameters)
{
	lpc_enetdata_t *lpc_enetif = pvParameters;
	u8_t idx;
//	UBaseType_t uxHighWaterMark;

	/* Inspect our own high water mark on entering the task. */
//	uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

	while (1)
	{
#if NO_SYS == 0
		/* Wait for transmit cleanup task to wakeup */
		sys_arch_sem_wait(&lpc_enetif->tx_clean_sem, 0);
#endif

		/* Error handling for TX underruns or errors. This should never happen
		 * unless something is holding the bus or the clocks are going too slow.
		 *  It can probably be safely removed. */
		if (LPC_EMAC->IntStatus & (EMAC_INT_TX_UNDERRUN | EMAC_INT_TX_ERR))
		{
			LINK_STATS_INC(link.err);
			LINK_STATS_INC(link.drop);

#if NO_SYS == 0
			/* Get exclusive access */
			sys_mutex_lock(&lpc_enetif->tx_lock_mutex);
#endif
			/* Reset the TX side */
			LPC_EMAC->MAC1 |= EMAC_MAC1_RES_TX;
//			LPC_EMAC->IntClear = (EMAC_INT_TX_UNDERRUN | EMAC_INT_TX_ERR);

			/* De-allocate all queued TX pbufs */
			for (idx = 0; idx < EMAC_NUM_TX_FRAG; idx++)
			{
				if (lpc_enetif->txb[idx] != NULL)
				{
					pbuf_free(lpc_enetif->txb[idx]);
					lpc_enetif->txb[idx] = NULL;
				}
			}

			/* Start TX side again */
			lpc_tx_setup(lpc_enetif);

#if NO_SYS == 0
			/* Restore access */
			sys_mutex_unlock(&lpc_enetif->tx_lock_mutex);
#endif
		}
		else
		{
			/* Calling the function will have used some stack space, we would
			therefore now expect uxTaskGetStackHighWaterMark() to return a
			value lower than when it was called on entering the task. */
//			uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL );
//
//			printf("\nTx Heap: %d", uxHighWaterMark);

			/* Free TX buffers that are done sending */
			lpc_tx_reclaim(lpc_enetdata.pnetif);
		}
	}
}
#endif

/* Low level init of the MAC and PHY */
static err_t low_level_init(struct netif *netif)
{
	lpc_enetdata_t *lpc_enetif = netif->state;
	err_t err = ERR_OK;
	u32_t tmp;
	u8_t tout;

	LPC_EMAC->IntEnable = 0;				/* Disable MAC interrupts */

	LPC_SC->PCONP |= (1 << 30);

    LPC_PINCON->PINSEL2 = 0x50150105;
    LPC_PINCON->PINSEL3 = (LPC_PINCON->PINSEL3 & ~0x0000000F) | 0x00000005;

    /* Initialize the PHY */
	tmp = SystemCoreClock / EMAC_MCFG_MII_MAXCLK;
	for (tout = 0; tout < sizeof(ethclkdiv); tout++)
	{
		if (ethclkdiv[tout] >= tmp) break;
	}

	LPC_EMAC->MCFG = ((tout & 0x0f) << 2) | EMAC_MCFG_RES_MII;	/* Save clock divider and Write to MII configuration register and reset */
	LPC_EMAC->MCFG &= ~(EMAC_MCFG_RES_MII);						/* Release reset */

	if (lpc_phy_init() == false)
	{
		return (false);
	}

    /* Resetting MAC Sublayers for TX and RX. See UM10360 p.159 */
    LPC_EMAC->MAC1 = EMAC_MAC1_RES_TX | EMAC_MAC1_RES_MCS_TX | EMAC_MAC1_RES_RX | EMAC_MAC1_RES_MCS_RX | EMAC_MAC1_SIM_RES | EMAC_MAC1_SOFT_RES;
    LPC_EMAC->Command = EMAC_CR_REG_RES | EMAC_CR_TX_RES | EMAC_CR_RX_RES;

	for (tout = 0; tout < 100; tout++) {}					/* Waiting a moment to reset. */

	/* Initial MAC configuration for: No RX Filter, full duplex, 100Mbps, inter-frame gap use default values */
	LPC_EMAC->MAC1 = EMAC_MAC1_PASS_ALL;
	LPC_EMAC->MAC2 = EMAC_MAC2_FULL_DUP | EMAC_MAC2_CRC_EN | EMAC_MAC2_PAD_EN;
	LPC_EMAC->Command = EMAC_CR_FULL_DUP | EMAC_CR_RMII;

	LPC_EMAC->IPGT = EMAC_IPGT_FULL_DUP;					/* Config for 100Mbs. */
	LPC_EMAC->SUPP = EMAC_SUPP_SPEED;						/* Config for 100Mbs. */
	LPC_EMAC->IPGR = (EMAC_IPGR_P2_DEF | EMAC_IPGR_P1_DEF);
	LPC_EMAC->MAXF = EMAC_ETH_MAX_FLEN;						/* Config. Max Frame Size. */
	LPC_EMAC->CLRT = EMAC_CLRT_DEF;

	/* Save station address */
	LPC_EMAC->SA0 = ((u32_t) netif->hwaddr[5] << 8) | ((u32_t) netif->hwaddr[4]);
	LPC_EMAC->SA1 = ((u32_t) netif->hwaddr[3] << 8) | ((u32_t) netif->hwaddr[2]);
	LPC_EMAC->SA2 = ((u32_t) netif->hwaddr[1] << 8) | ((u32_t) netif->hwaddr[0]);

	/* Setup transmit and receive descriptors */
	if (lpc_tx_setup(lpc_enetif) != ERR_OK)
	{
		return ERR_BUF;
	}

	if (lpc_rx_setup(lpc_enetif) != ERR_OK)
	{
		return ERR_BUF;
	}

	/* Enable configuration filters. */
#if MAC_NO_FILTER
	LPC_EMAC->Command |= (EMAC_CR_PASS_RUNT_FRM | EMAC_CR_PASS_RX_FILT);	/* Disable all filters. */
#else
	LPC_EMAC->RxFilterCtrl = (EMAC_RFC_PERFECT_EN | EMAC_RFC_UCAST_EN);

#if LWIP_IGMP
	LPC_EMAC->RxFilterCtrl |= EMAC_RFC_MCAST_EN;
#endif

#if !IP_SOF_BROADCAST_RECV
	LPC_EMAC->RxFilterCtrl |= EMAC_RFC_BCAST_EN;
#endif

	LPC_EMAC->Command &= ~EMAC_CR_PASS_RX_FILT;
#endif

	/* Enable MAC interrupts only after LWIP is ready */
	NVIC_SetPriority(ENET_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
	NVIC_EnableIRQ(ENET_IRQn);

	/* Clear and enable rx/tx interrupts */
	LPC_EMAC->IntClear = 0xffff;						/* Clear all MAC interrupts */
	LPC_EMAC->IntEnable = (EMAC_INT_TX_UNDERRUN | EMAC_INT_TX_ERR | EMAC_INT_TX_DONE);	/* Enabling TX Interrupts. */
	LPC_EMAC->IntEnable |= (EMAC_INT_RX_OVERRUN | EMAC_INT_RX_ERR | EMAC_INT_RX_DONE);	/* Enabling RX Interrupts. */

	/* Enable RX and TX */
	LPC_EMAC->MAC1 |= EMAC_MAC1_REC_EN;
	LPC_EMAC->Command |= (EMAC_CR_TX_EN | EMAC_CR_RX_EN);

	return (err);
}

/* This function is the ethernet packet send function. It calls etharp_output after checking link status. */
static err_t lpc_etharp_output(struct netif *netif, struct pbuf *q, ip_addr_t *ipaddr)
{
	/* Only send packet is link is up */
	if(netif->flags & NETIF_FLAG_LINK_UP)
	{
		return (etharp_output(netif, q, ipaddr));
	}

	return ERR_CONN;
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/
/* Attempt to allocate and requeue a new pbuf for RX */
u8_t lpc_rx_queue(struct netif *netif)
{
	lpc_enetdata_t *lpc_enetif = netif->state;
	struct pbuf *p = NULL;
	u8_t queued = 0;

	/* Attempt to requeue as many packets as possible */
	while (lpc_enetif->rx_free_descs > 0)
	{
		/* Allocate a pbuf from the pool. We need to allocate at the maximum size as we don't know the size of the yet to be received packet. */
		p = pbuf_alloc(PBUF_RAW, (u16_t) EMAC_ETH_MAX_FLEN, PBUF_RAM);

		printf("\nRTOS Heap: %d\n", xPortGetFreeHeapSize());

		if (p == NULL)
		{
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE, ("lpc_rx_queue: could not allocate RX pbuf (free desc=%d)\n", lpc_enetif->rx_free_descs));
			return (queued);
		}

		/* pbufs allocated from the RAM pool should be non-chained. */
		LWIP_ASSERT("lpc_rx_queue: pbuf is not contiguous (chained)", pbuf_clen(p) <= 1);

		/* Queue packet */
		lpc_rxqueue_pbuf(lpc_enetif, p);

		/* Update queued count */
		queued++;
	}

	return (queued);
}

/* Attempt to read a packet from the EMAC interface */
void lpc_enetif_input(struct netif *netif)
{
	struct eth_hdr *ethhdr;
	struct pbuf *p = NULL;

	/* move received packet into a new pbuf */
	p = lpc_low_level_input(netif);

	if (p == NULL)
	{
		return;
	}

	/* points to packet payload, which starts with an Ethernet header */
	ethhdr = p->payload;

	switch (htons(ethhdr->type))
	{
		case ETHTYPE_IP:
		case ETHTYPE_ARP:
#if PPPOE_SUPPORT
		case ETHTYPE_PPPOEDISC:
		case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
		/* full packet send to tcpip_thread to process */
		if (netif->input(p, netif) != ERR_OK)
		{
			LWIP_DEBUGF(NETIF_DEBUG, ("lpc_enetif_input: IP input error\n"));
			pbuf_free(p);	/* Free buffer */
			p = NULL;
		}
		break;
		default:
			pbuf_free(p);	/* Return buffer */
			p = NULL;
		break;
	}
}

/* Call for freeing TX buffers that are complete */
void lpc_tx_reclaim(struct netif *netif)
{
	lpc_tx_reclaim_st((lpc_enetdata_t *) netif->state, (u8_t *)LPC_EMAC->TxConsumeIndex);
}

/* Polls if an available TX descriptor is ready */
static u8_t lpc_tx_ready(struct netif *netif)
{
	u8_t pidx, cidx;

	cidx = (u8_t) LPC_EMAC->TxConsumeIndex;
	pidx = (u8_t) LPC_EMAC->TxProduceIndex;

	if(pidx == cidx)
	{
		return(EMAC_NUM_TX_FRAG - 1);
	}
	if(cidx > pidx)
	{
		return(EMAC_NUM_TX_FRAG - 1 - ((EMAC_NUM_TX_FRAG - cidx) + pidx));
	}
	return(EMAC_NUM_TX_FRAG - 1 - (pidx - cidx));
}

/**
 * @brief	EMAC interrupt handler
 * @return	Nothing
 * @note	This function handles the transmit, receive, and error interrupt of
 * the LPC17xx/40xx. This is meant to be used when NO_SYS=0.
 */
void ENET_IRQHandler(void)
{
	portBASE_TYPE xRecTaskWoken = pdFALSE;
	portBASE_TYPE XTXTaskWoken = pdFALSE;
	u32_t ints;

	/* Interrupts are of 2 groups - transmit or receive. Based on the interrupt, kick off the receive or transmit (cleanup) task */
	ints = LPC_EMAC->IntStatus & 0x000030ff;	/* Get pending interrupts only */

	if (ints & (EMAC_INT_RX_OVERRUN | EMAC_INT_RX_ERR | EMAC_INT_RX_DONE))
	{
		/* RX group interrupt(s) */
		/* Give semaphore to wakeup RX receive task. Note the FreeRTOS method is used instead of the LWIP arch method. */
		xSemaphoreGiveFromISR(lpc_enetdata.rx_sem, &xRecTaskWoken);
	}

	if (ints & (EMAC_INT_TX_UNDERRUN | EMAC_INT_TX_ERR | EMAC_INT_TX_DONE))
	{
		/* TX group interrupt(s) */
		/* Give semaphore to wakeup TX cleanup task. Note the FreeRTOS method is used instead of the LWIP arch method. */
		xSemaphoreGiveFromISR(lpc_enetdata.tx_clean_sem, &XTXTaskWoken);
	}

	/* Clear pending interrupts */
	LPC_EMAC->IntClear = ints;

	/* Context switch needed? */
	portYIELD_FROM_ISR(xRecTaskWoken || XTXTaskWoken);
}

/* Set up the MAC interface duplex */
void lpc_emac_set_duplex(u8_t full_duplex)
{
	if (full_duplex)
	{
		LPC_EMAC->IPGT = EMAC_IPGT_FULL_DUP;
		LPC_EMAC->MAC2 |= EMAC_MAC2_FULL_DUP;
		LPC_EMAC->Command |= EMAC_CR_FULL_DUP;
	}
	else
	{
		LPC_EMAC->IPGT = EMAC_IPGT_HALF_DUP;
		LPC_EMAC->MAC2 &= ~EMAC_MAC2_FULL_DUP;
		LPC_EMAC->Command &= ~EMAC_CR_FULL_DUP;
	}
}

/* Set up the MAC interface speed */
void lpc_emac_set_speed(u8_t mbs_100)
{
	if (mbs_100)
	{
		LPC_EMAC->SUPP = EMAC_SUPP_SPEED;
	}
	else
	{
		LPC_EMAC->SUPP = 0;
	}
}

/* LWIP 17xx/40xx EMAC initialization function */
err_t lpc_enetif_init(struct netif *netif)
{
	const u8_t boardmac[] = {MAC05, MAC04, MAC03, MAC02, MAC01, MAC00};
	err_t err;

	LWIP_ASSERT("netif != NULL", (netif != NULL));

	lpc_enetdata.pnetif = netif;

	/* set MAC hardware address */
	netif->hwaddr_len = ETHARP_HWADDR_LEN;
	memcpy(netif->hwaddr, boardmac, netif->hwaddr_len);

	/* maximum transfer unit */
	netif->mtu = IFMTUSIZE;

	netif->flags = NETIF_FLAG_ETHARP | NETIF_FLAG_UP | NETIF_FLAG_ETHERNET;

	/* device capabilities */
#if LWIP_IGMP
	netif->flags |= NETIF_FLAG_IGMP;
#endif

#if !IP_SOF_BROADCAST
	netif->flags |= NETIF_FLAG_BROADCAST;
#endif

	/* Initialize the hardware */
	netif->state = &lpc_enetdata;

	err = low_level_init(netif);
	if (err != ERR_OK)
	{
		return err;
	}

#if LWIP_NETIF_HOSTNAME
	netif->hostname = "lwip";	/* Initialize interface hostname */
#endif

	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;

	netif->output = lpc_etharp_output;
	netif->linkoutput = lpc_low_level_output;

	/* For FreeRTOS, start tasks */
#if NO_SYS == 0
	lpc_enetdata.xtx_count_sem = xSemaphoreCreateCounting(EMAC_NUM_TX_FRAG, EMAC_NUM_TX_FRAG);
	LWIP_ASSERT("xtx_count_sem creation error", (lpc_enetdata.xtx_count_sem != NULL));
	err = sys_mutex_new(&lpc_enetdata.tx_lock_mutex);
	LWIP_ASSERT("tx_lock_mutex creation error", (err == ERR_OK));
	if(err != ERR_OK) return (ERR_MEM);

#if LOCK_RX_THREAD
	err = sys_mutex_new(&lpc_enetdata.rx_lock_mutex);
	LWIP_ASSERT("rx_lock_mutex creation error", (err == ERR_OK));
	if(err != ERR_OK) return (ERR_MEM);
#endif

	/* Packet receive task */
	err = sys_sem_new(&lpc_enetdata.rx_sem, 0);
	LWIP_ASSERT("rx_sem creation error", (err == ERR_OK));
	if(err != ERR_OK) return (ERR_MEM);

	if(sys_thread_new("lwiprx_thread", vPacketReceiveTask, netif->state, DEFAULT_THREAD_STACKSIZE, tskRECPKT_PRIORITY) == NULL) return (ERR_MEM);

	/* Transmit cleanup task */
	err = sys_sem_new(&lpc_enetdata.tx_clean_sem, 0);
	LWIP_ASSERT("tx_clean_sem creation error", (err == ERR_OK));
	if(err != ERR_OK) return (ERR_MEM);

	if(sys_thread_new("lwiptx_thread", vTransmitCleanupTask, netif->state, DEFAULT_THREAD_STACKSIZE, tskTXCLEAN_PRIORITY) == NULL) return (ERR_MEM);

#endif

	return (ERR_OK);
}

/**
 * @}
 */
