#include <string.h>
#include <time.h>
#include <stdio.h>

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

#include "lpc17xx_rtc.h"
#include "lpc17xx_uart.h"


#if LWIP_NETCONN /* don't build if not configured for use in lwipopts.h */

/** This is an example of a "SNTP" client (with netconn API).
 *
 * For a list of some public NTP servers, see this link :
 * http://support.ntp.org/bin/view/Servers/NTPPoolServers
 *
 */

#include "sntp.h"

#ifndef SNTP_DEBUG
#define SNTP_DEBUG         LWIP_DBG_OFF
#endif

static TaskHandle_t sntphandle = NULL;

static void xSntpTask(void *pvParameters);

static void xSntpTask(void *pvParameters)
{
	u8_t *sntp_request;
	u8_t *sntp_response;
	time_t timestamp = 0;
	ip4_addr_t sntp_server_address;
	RTC_TIME_Type rtcclock;
	struct netconn * sendUDPNetConn;
	struct netbuf *sendUDPNetBuf;
	struct netbuf *receiveUDPNetBuf;
	u16_t dataLen;

	rtcHardwareStart();		/* Inicializando o RTC. */

	while(1)
	{
		timestamp = 0;
		sendUDPNetConn = netconn_new(NETCONN_UDP);	/* create new socket */
		if (sendUDPNetConn != NULL)
		{
			ip4addr_aton(SNTP_SERVER_ADDRESS, &sntp_server_address);
			if(netconn_connect(sendUDPNetConn, &sntp_server_address, SNTP_PORT) == ERR_OK)
			{
				sendUDPNetBuf = netbuf_new();
				sntp_request = (u8_t *) netbuf_alloc(sendUDPNetBuf, SNTP_MAX_DATA_LEN);	/* Create data space for netbuf, if we can. */
				memset(sntp_request, 0, SNTP_MAX_DATA_LEN);								/* Prepare SNTP request */
				sntp_request[0] = SNTP_LI_NO_WARNING | SNTP_VERSION | SNTP_MODE_CLIENT;
				// Send SNTP request to server.
				if (netconn_send(sendUDPNetConn, sendUDPNetBuf) == ERR_OK)
				{
					sendUDPNetConn->recv_timeout = SNTP_RECV_TIMEOUT;	/* Set recv timeout. */
					netconn_recv(sendUDPNetConn, &receiveUDPNetBuf);	/* Receive SNTP server response. */
					if (receiveUDPNetBuf != NULL)
					{
						netbuf_data(receiveUDPNetBuf, (void **) &sntp_response,(u16_t *) &dataLen);	/* Get pointer to response data. */
						// If the response size is good.
						if (dataLen == SNTP_MAX_DATA_LEN)
						{
							// If this is a SNTP response...
							if (((sntp_response[0] & SNTP_MODE_MASK) == SNTP_MODE_SERVER) || ((sntp_response[0] & SNTP_MODE_MASK) == SNTP_MODE_BROADCAST))
							{
								/* extract GMT time from response */
								memcpy(&timestamp, (sntp_response + SNTP_RCV_TIME_OFS), sizeof(timestamp));
								timestamp = (ntohl(timestamp) - DIFF_SEC_1900_1970);
								LWIP_DEBUGF(SNTP_DEBUG, "Received timestamp.");
							}
						}
					}
					netbuf_delete(receiveUDPNetBuf);
				}
				netbuf_delete(sendUDPNetBuf);
			}
		}
		netconn_delete(sendUDPNetConn);

		{	/* New context to gain memory. */
			u8_t hour, min, sec;

			printf("\nRTOS Heap: %d\n", xPortGetFreeHeapSize());

			stats_display();

			if(timestamp != 0)
			{
				struct tm *timentp;

				/* Correct Fuse! */
				timestamp += ((-3) * 3600);
				printf("\nNTP Time (Now %d): %s", -3, ctime(&timestamp));

				/* Programming Internal RTC. */
				timentp = gmtime(&timestamp);
				RTC_SetTime(LPC_RTC, RTC_TIMETYPE_SECOND, timentp->tm_sec);
				RTC_SetTime(LPC_RTC, RTC_TIMETYPE_MINUTE, timentp->tm_min);
				RTC_SetTime(LPC_RTC, RTC_TIMETYPE_HOUR, timentp->tm_hour);
				RTC_SetTime(LPC_RTC, RTC_TIMETYPE_MONTH, timentp->tm_mon);
				RTC_SetTime(LPC_RTC, RTC_TIMETYPE_YEAR, timentp->tm_year);
				RTC_SetTime(LPC_RTC, RTC_TIMETYPE_DAYOFMONTH, timentp->tm_mday);
				RTC_SetTime(LPC_RTC, RTC_TIMETYPE_DAYOFWEEK, timentp->tm_wday);
				RTC_SetTime(LPC_RTC, RTC_TIMETYPE_DAYOFYEAR, timentp->tm_yday);

				/* Now.. Programming next check. */
				timestamp += (SNTP_UPDATE_DELAY / 1000);
				timentp = gmtime(&timestamp);

				hour = timentp->tm_hour;
				min = timentp->tm_min;
				sec = timentp->tm_sec;
			}
			else
			{
				printf("\nError: I can't get SNTP data.\n");

				/* Scheduling next ntp update. */
				RTC_GetFullTime(LPC_RTC, &rtcclock);

//				rtcclock.MIN += 1; /* Quando não sincroniza. Busca a cada 1 minuto. */
				rtcclock.SEC += 10; /* Quando não sincroniza. Busca a cada 10s. */

				if(rtcclock.SEC >= 60)
				{
					rtcclock.MIN += 1;
					rtcclock.SEC = 0;
				}

				if(rtcclock.MIN >= 60)
				{
					rtcclock.HOUR += 1;
					rtcclock.MIN = 0;
				}

				if(rtcclock.HOUR >= 24)
				{
					rtcclock.HOUR = 0;
				}

				hour = rtcclock.HOUR;
				min = rtcclock.MIN;
				sec = rtcclock.SEC;
			}

			RTC_SetAlarmTime (LPC_RTC, RTC_TIMETYPE_HOUR, hour);
			RTC_SetAlarmTime (LPC_RTC, RTC_TIMETYPE_MINUTE, min);
			RTC_SetAlarmTime (LPC_RTC, RTC_TIMETYPE_SECOND, sec);
		}
		vTaskSuspend(sntphandle);
	}
}

/*****************************************************************************/
TaskHandle_t sntpGetHandle(void)
{
	return (sntphandle);
}

/*****************************************************************************/
void sntpSetHandle(TaskHandle_t handle)
{
	sntphandle = handle;
}

/*****************************************************************************/
void rtcHardwareStart(void)
{
	RTC_Init(LPC_RTC);

	NVIC_DisableIRQ(RTC_IRQn);
	NVIC_SetPriority(RTC_IRQn, ((0x01<<3)|0x01));

	// Habilita RTC
	RTC_Cmd(LPC_RTC, ENABLE);
	RTC_CalibCounterCmd(LPC_RTC, DISABLE);

	// Configura o CIIR para contador de segundo para interrupção
	RTC_CntIncrIntConfig (LPC_RTC, RTC_TIMETYPE_SECOND, ENABLE);

	// Configura o AMR para contador de minutos e hora para interrupção de alarme
	LPC_RTC->AMR = 0b11111000;

	// Habilita interrupção RTC
	NVIC_EnableIRQ(RTC_IRQn);
}

/*****************************************************************************/
void RTC_IRQHandler(void)
{
	// Isto é o incremento do contador de interrupção
	if (RTC_GetIntPending(LPC_RTC, RTC_INT_COUNTER_INCREASE))
	{
		RTC_ClearIntPending(LPC_RTC, RTC_INT_COUNTER_INCREASE);	/* Limpa interrupção pendente. */
	}

	/* Checa a interrupção de alarme. */
	if (RTC_GetIntPending(LPC_RTC, RTC_INT_ALARM))
	{
		/* Resume task to update clock using SNTP. */
		xTaskResumeFromISR(sntpGetHandle());
		RTC_ClearIntPending(LPC_RTC, RTC_INT_ALARM);	/* Limpa interrupção pendente. */
	}
}

/*****************************************************************************/
sys_thread_t sntpInit(void *sntpconfig, u16_t stacksize, u8_t taskprio)
{
	sntphandle = sys_thread_new("sntpsrv", xSntpTask, sntpconfig, stacksize, taskprio);
	return(sntphandle);
}

#endif /* LWIP_SOCKET */
