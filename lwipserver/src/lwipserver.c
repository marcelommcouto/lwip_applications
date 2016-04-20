/*
===============================================================================
 Name        : lwipserver.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include "FreeRTOS.h"
#include "task.h"

#include <cr_section_macros.h>
#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "string.h"

// TODO: insert other include files here
/* Lwip Includes. */
#include "lwip/debug.h"
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/api.h"
#include "lwip/stats.h"
#include "lwip/dhcp.h"
#include "netif/etharp.h"

#include "phy_smsc87x0.h"
#include "freertos_lwip_mac.h"

#include "lwipopts.h"

#include "shell/shell.h"
#include "ftpclient/ftpclient.h"
#include "sntp/sntp.h"

/* For monitoring. */
#include "lpc17xx_pinsel.h"
#include "lpc17xx_uart.h"

// TODO: insert other definitions and declarations here
static void blinkLed(void *pvParameters);
static void vEthernetTask(void *pvParameters);

int main(void)
{
	SystemInit();

	SystemCoreClockUpdate();

	/* Monitor ON. */

	UART_CFG_Type UARTConfigStruct;	/* UART Configuration structure variable. */
	PINSEL_CFG_Type PinCfg;			/* Pin configuration for UART0. */

	PinCfg.Funcnum = 1;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 2;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 3;
	PINSEL_ConfigPin(&PinCfg);

	UART_ConfigStructInit(&UARTConfigStruct);
	UARTConfigStruct.Baud_rate = 19200;
	UART_Init(LPC_UART0, &UARTConfigStruct);
	UART_TxCmd(LPC_UART0, ENABLE);

	UART_Send(LPC_UART0, "Monitor Enabled.\n", sizeof("\nMonitor Enabled."), BLOCKING);

	/* Monitor On. */

	xTaskCreate(blinkLed, "blink", configMINIMAL_STACK_SIZE * 2, NULL, DEFAULT_THREAD_PRIO, NULL);				/* create task to blink led */
	xTaskCreate(vEthernetTask, "network", (configMINIMAL_STACK_SIZE * 4), NULL, DEFAULT_THREAD_PRIO, NULL);	/* create task to Ethernet */

	vTaskStartScheduler();		/* Start the scheduler. */

    return 0 ;
}

/****************************************************************************/
static void blinkLed(void *pvParameters)
{
//	uint8_t cont = 0;

	LPC_GPIO0->FIODIR |= (1 << 4);

    while(1)
    {
        LPC_GPIO0->FIOSET |= (1 << 4);
        vTaskDelay(500/portTICK_RATE_MS);
        LPC_GPIO0->FIOCLR |= (1 << 4);
        vTaskDelay(500/portTICK_RATE_MS);
//        cont++;
//        if(cont > 10)
//        {
//        	printf(" %d\n", xPortGetFreeHeapSize());
//        	cont = 0;
//        }
    }
    vTaskDelete(NULL);
}

/****************************************************************************/
static void vEthernetTask(void *pvParameters)
{
	ip4_addr_t ipaddr, netmask, gw;
	struct netif lpc17xx_netif;
	bool prt_ip = false;
	uint32_t physts;
	sys_thread_t hldsntp = NULL;

//	UBaseType_t uxHighWaterMark;
//	uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );	/* Inspect our own high water mark on entering the task. */

#if LWIP_DHCP
	ip4_addr_set_zero(&ipaddr);
	ip4_addr_set_zero(&netmask);
	ip4_addr_set_zero(&gw);
#else
	IP4_ADDR(&ipaddr, 192, 168, 1, 2);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
	IP4_ADDR(&gw, 192, 168, 1, 1);
#endif

	tcpip_init(NULL, NULL);				/* Initialize TCP/IP Stack. */
	netif_add(&lpc17xx_netif, &ipaddr, &netmask, &gw, NULL, lpc_enetif_init, tcpip_input);
	netif_set_default(&lpc17xx_netif);	/* Setting this interface default. */

#if !LWIP_DHCP
	netif_set_up(&lpc17xx_netif);
#else
	dhcp_start(&lpc17xx_netif);
#endif

//	shellInit(NULL, (configMINIMAL_STACK_SIZE * 3), DEFAULT_THREAD_PRIO);

	/* This loop monitors the PHY link and will handle cable events via the PHY driver. */
	while (1)
	{
		/* Call the PHY status update state machine once in a while to keep the link status up-to-date */
		physts = lpcPHYStsPoll();

		/* Only check for connection state when the PHY status has changed */
		if (physts & PHY_LINK_CHANGED)
		{
//			dhcp_network_changed(&lpc17xx_netif);

			if (physts & PHY_LINK_CONNECTED)
			{
				prt_ip = false;

				/* Set interface speed and duplex */
				if (physts & PHY_LINK_SPEED100)
				{
					lpc_emac_set_speed(1);
				}
				else
				{
					lpc_emac_set_speed(0);
				}
				if (physts & PHY_LINK_FULLDUPLX)
				{
					lpc_emac_set_duplex(1);
				}
				else
				{
					lpc_emac_set_duplex(0);
				}
				tcpip_callback_with_block((tcpip_callback_fn) netif_set_link_up, (void *) &lpc17xx_netif, 1);
			}
			else
			{
				tcpip_callback_with_block((tcpip_callback_fn) netif_set_link_down, (void *) &lpc17xx_netif, 1);
				//
				if(hldsntp != NULL)
				{
					vTaskDelete(hldsntp);
					hldsntp = NULL;
					UART_Send(LPC_UART0, "\nSNTP Off.", sizeof("\nSNTP Off."), NONE_BLOCKING);
				}
			}
			vTaskDelay(configTICK_RATE_HZ);		/* Delay for link detection (500mS) */
		}
		//
//		uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
//		printf("\nHeap: %d", uxHighWaterMark);

		/* Print IP address info */
		if (!prt_ip)
		{
			if (lpc17xx_netif.ip_addr.addr)
			{
				uint8_t tmp_buff[16];

				prt_ip = true;

				UART_Send(LPC_UART0, "\nIP:", sizeof("\nIP:"), BLOCKING);
				ip4addr_ntoa_r((const ip_addr_t *)&lpc17xx_netif.ip_addr, tmp_buff, 16);
				UART_Send(LPC_UART0, &tmp_buff, sizeof(tmp_buff), BLOCKING);
				UART_Send(LPC_UART0, "\nMask:", sizeof("\nMask:"), BLOCKING);
				ip4addr_ntoa_r((const ip_addr_t *)&lpc17xx_netif.netmask, tmp_buff, 16);
				UART_Send(LPC_UART0, &tmp_buff, sizeof(tmp_buff), BLOCKING);
				UART_Send(LPC_UART0, "\nGate:", sizeof("\nGate:"), BLOCKING);
				ip4addr_ntoa_r((const ip_addr_t *)&lpc17xx_netif.gw, tmp_buff, 16);
				UART_Send(LPC_UART0, &tmp_buff, sizeof(tmp_buff), BLOCKING);

				if(hldsntp == NULL)
				{
					hldsntp = sntpInit(NULL, (configMINIMAL_STACK_SIZE * 3), DEFAULT_THREAD_PRIO);
					UART_Send(LPC_UART0, "\nSNTP On.", sizeof("\nSNTP On."), BLOCKING);
				}
			}
		}
	}
}
