/*
 * @brief LPC17xx/LPC40xx EMAC and PHY driver configuration file for LWIP
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

#ifndef __FREERTOS_LWIP_MAC_H_
#define __FREERTOS_LWIP_MAC_H_

#include "lwip/opt.h"
#include "lwip/err.h"
#include "lpc17xx_emac.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @defgroup NET_LWIP_LPC17XX40XX_EMAC_DRIVER 17xx/40xx EMAC driver for LWIP
 * @ingroup NET_LWIP
 * @note	This is the LPC17xx/40xx EMAC driver for LWIP. This driver supports both
 * RTOS-based and no-RTOS operation with LWIP. WHen using an RTOS, several
 * threads will be created for handling RX and TX packet fucntions.
 *
 * Note that some LWIP examples may not necessarily use all the provided
 * LWIP driver functions or may contain overriden versions of the functions.
 * (For example, PHY drives may have their own implementation of the MII
 * read/write functions).
 * @{
 */

/**
 * @brief Driver transmit and receive thread priorities
 * Thread priorities for receive thread and TX cleanup thread. Alter
 * to prioritize receive or transmit bandwidth. In a heavily loaded
 * system or with LWIP_DEBUG enabled, the priorities might be better
 * the same. */
#define tskRECPKT_PRIORITY   	(TCPIP_THREAD_PRIO - 1)
#define tskTXCLEAN_PRIORITY  	(TCPIP_THREAD_PRIO - 1)

/* TODO: Read this data below from FLASH! */
#define MAC05 					0x00
#define MAC04 					0x1A
#define MAC03 					0x3F
#define MAC02 					0x08
#define MAC01 					0x80
#define MAC00 					0xAA

/* Define those to better describe your network interface. */
#define IFNAME0 				'e'
#define IFNAME1 				'n'

/* Define MTU (Maximum Transfer Unit) for this interface. */
#define IFMTUSIZE				(EMAC_ETH_MAX_FLEN - 36)//1500

/* Autonegotiation mode enable flag */
#define PHY_USE_AUTONEG 		1

/* PHY interface full duplex operation or half duplex enable flag.
   Only applies if PHY_USE_AUTONEG = 0 */
#define PHY_USE_FULL_DUPLEX 	1

/* PHY interface 100MBS or 10MBS enable flag.
   Only applies if PHY_USE_AUTONEG = 0 */
#define PHY_USE_100MBS 			1

/**
 * @brief	Attempt to read a packet from the EMAC interface
 * @param	netif	: lwip network interface structure pointer
 * @return	Nothing
 */
void lpc_enetif_input(struct netif *netif);

/**
 * @brief	Attempt to allocate and requeue a new pbuf for RX
 * @param	netif	: lwip network interface structure pointer
 * @return	The number of new descriptors queued
 */
u8_t lpc_rx_queue(struct netif *netif);

/**
 * @brief	Call for freeing TX buffers that are complete
 * @param	netif	: lwip network interface structure pointer
 * @return	Nothing
 */
void lpc_tx_reclaim(struct netif *netif);

/**
 * @brief	LWIP 17xx/40xx EMAC initialization function
 * @param	netif	: lwip network interface structure pointer
 * @return	ERR_OK if the loopif is initialized, or ERR_* on other errors
 * @note	Should be called at the beginning of the program to set up the
 * network interface. This function should be passed as a parameter to
 * netif_add().
 */
err_t lpc_enetif_init(struct netif *netif);

/**
 * @brief	Set up the MAC interface duplex
 * @param	full_duplex	: 0 = half duplex, 1 = full duplex
 * @return	Nothing
 * @note	This function provides a method for the PHY to setup the EMAC
 * for the PHY negotiated duplex mode.
 */
void lpc_emac_set_duplex(u8_t full_duplex);

/**
 * @brief	Set up the MAC interface speed
 * @param	mbs_100	: 0 = 10mbs mode, 1 = 100mbs mode
 * @return	Nothing
 * @note	This function provides a method for the PHY to setup the EMAC
 * for the PHY negotiated bit rate.
 */
void lpc_emac_set_speed(u8_t mbs_100);

/**
 * @brief	Millisecond Delay function
 * @param	ms		: Milliseconds to wait
 * @return	None
 */
extern void msDelay(uint32_t ms);

s8_t writephy(u32_t phyreg, u8_t address, u16_t value);
s32_t readphy(u32_t phyreg, u8_t address);

#ifdef __cplusplus
}
#endif

#endif /* __LPC_17XX40XX_EMAC_CONFIG_H_ */
