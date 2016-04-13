/*
 * @brief SMSC 87x0 simple PHY driver
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

#include "stdint.h"
#include "stdbool.h"

#include "lpc17xx_emac.h"
#include "freertos_lwip_mac.h"
#include "LPC17xx.h"
#include "FreeRTOS.h"
#include "task.h"

#include <phy_smsc87x0.h>

/** @defgroup SMSC87X0_PHY BOARD: PHY status and control driver for the SMSC 87x0
 * @ingroup BOARD_PHY
 * Various functions for controlling and monitoring the status of the
 * SMSC 87x0 PHY.
 * @{
 */

/* DP83848 PHY update flags */
static uint32_t physts, olddphysts;

/* PHY update counter for state machine */
static int32_t phyustate;

/**
 * @brief	MilliSecond delay function based on FreeRTOS
 * @param	ms	: Number of milliSeconds to delay
 * @return	Nothing
 * Needed for some functions, do not use prior to FreeRTOS running
 */
void msDelay(uint32_t ms)
{
	vTaskDelay((configTICK_RATE_HZ * ms) / 1000);
}

/* Update PHY status from passed value */
static void smsc_update_phy_sts(uint16_t linksts, uint16_t sdsts)
{
	/* Update link active status */
	if (linksts & EMAC_PHY_BMSR_LINK_ESTABLISHED)
	{
		physts |= PHY_LINK_CONNECTED;
	}
	else
	{
		physts &= ~PHY_LINK_CONNECTED;
	}

	switch (sdsts & LAN8_SPEEDMASK)
	{
		case LAN8_SPEED100F:
		default:
			physts |= PHY_LINK_SPEED100;
			physts |= PHY_LINK_FULLDUPLX;
		break;
		case LAN8_SPEED10F:
			physts &= ~PHY_LINK_SPEED100;
			physts |= PHY_LINK_FULLDUPLX;
		break;
		case LAN8_SPEED100H:
			physts |= PHY_LINK_SPEED100;
			physts &= ~PHY_LINK_FULLDUPLX;
		break;
		case LAN8_SPEED10H:
			physts &= ~PHY_LINK_SPEED100;
			physts &= ~PHY_LINK_FULLDUPLX;
		break;
	}

	/* If the status has changed, indicate via change flag */
	if ((physts & (PHY_LINK_SPEED100 | PHY_LINK_FULLDUPLX | PHY_LINK_CONNECTED)) !=
		(olddphysts & (PHY_LINK_SPEED100 | PHY_LINK_FULLDUPLX | PHY_LINK_CONNECTED))) {
		olddphysts = physts;
		physts |= PHY_LINK_CHANGED;
	}
}

/* Initialize the DP83848C PHY in RMII mode. */
u32_t lpc_phy_init(void)
{
	u16_t tmp;
	s16_t i;

	/* Initial states for PHY status and state machine */
	olddphysts = physts = phyustate = 0;

	/* Only first read and write are checked for failure */
	/* Put the DP83848C in reset mode and wait for completion */
	if (writephy(EMAC_PHY_REG_BMCR, PHYADDR, EMAC_PHY_BMCR_RESET) != 0)
	{
		return (ERROR);
	}

	i = 400;
	while (i > 0)
	{
		msDelay(1);
		tmp = readphy(EMAC_PHY_REG_BMCR, PHYADDR);
		if (tmp == ERR_MEM)
		{
			return (ERROR);
		}

		if (!(tmp & (EMAC_PHY_BMCR_RESET | EMAC_PHY_BMCR_POWERDOWN)))
		{
			i = -1;
		}
		else
		{
			i--;
		}
	}
	/* Timeout? */
	if (i == 0)
	{
		return (ERROR);
	}

	/* Setup link */
	writephy(EMAC_PHY_REG_BMCR, PHYADDR, EMAC_PHY_BMCR_AN);

	/* The link is not set active at this point, but will be detected later */
	return (SUCCESS);
}

/* Phy status update state machine */
uint32_t lpcPHYStsPoll(void)
{
	static uint16_t sts;
	static u32_t temp;

	switch (phyustate)
	{
		default:
		case 0:
			/* Read value at PHY address and register */
			sts = readphy(EMAC_PHY_REG_BMSR, PHYADDR);
			if (sts != ERR_MEM)
			{
				physts &= ~PHY_LINK_CHANGED;
				physts = physts | PHY_LINK_BUSY;
				phyustate = 1;
			}
		break;
		case 1:
			temp = readphy(DP83848_PHYSTS_REG, PHYADDR);
			phyustate = 2;
		break;
		case 2:
			/* Update PHY status */
			physts &= ~PHY_LINK_BUSY;
			smsc_update_phy_sts(sts, temp);
			phyustate = 0;
		break;
	}
	return (physts);
}

/**
 * @}
 */
