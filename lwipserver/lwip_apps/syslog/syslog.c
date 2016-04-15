/** 
 *   Modulo: syslog
 *   @file syslog.c
 *   Veja syslog.h para mais informações.
 ******************************************************************************/

/*******************************************************************************
 *                             MODULOS UTILIZADOS							   *
 ******************************************************************************/

/*
 * Inclusão de arquivos de cabeçalho da ferramenta de desenvolvimento.
 * Por exemplo: '#include <stdlib.h>'.
 */
#include <stdint.h>   /* Para as definições de uint8_t/uint16_t */
#include <stdbool.h>  /* Para as definições de true/false */ 

/*
 * Inclusão de arquivos de cabeçalho sem um arquivo ".c" correspondente.
 * Por exemplo: 
 * #include "stddefs.h" 
 * #include "template_header.h"
 */

/*
 * Inclusão de arquivos de cabeçalho de outros módulos utilizados por este.
 * Por exemplo: '#include "serial.h"'.
 */
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "lpc17xx_rtc.h"

/*
 * Inclusão dos arquivos ".tab.h" deste módulo.
 * Por exemplo: 
 * #include "barcode.tab.h"
 * #include "template.tab.h"
 */

/*
 * Inclusão do arquivo de cabeçalho deste módulo.
 */
#include "syslog.h"

/*******************************************************************************
 *                     CONSTANTES E DEFINICOES DE MACRO						   *
 ******************************************************************************/
#ifndef SYSLOG_DEBUG
#define SYSLOG_DEBUG         LWIP_DBG_OFF
#endif

/*******************************************************************************
 *                      ESTRUTURAS E DEFINIÇÕES DE TIPOS					   *
 ******************************************************************************/

/*******************************************************************************
 *                        VARIÁVEIS PUBLICAS (Globais)						   *
 ******************************************************************************/

/*******************************************************************************
 *                  DECLARACOES DE VARIAVEIS PRIVADAS (static)				   *
 ******************************************************************************/
static TaskHandle_t syslogHandle = NULL;
static syslogmsg_t *message, *curwp, *first;

/*******************************************************************************
 *                   PROTOTIPOS DAS FUNCOES PRIVADAS (static)				   *
 ******************************************************************************/
static void syslogTask(void *pvParameters);

/*******************************************************************************
 *                      IMPLEMENTACAO DAS FUNCOES PUBLICAS					   *
 ******************************************************************************/
void syslogSend(const s8_t *msg, u8_t severity)
{
	char *tmptext = (s8_t *)mem_malloc(sizeof(s8_t )*MESSAGESIZE);	/* Allocate space with the size of msg text. */
	if(syslogHandle != NULL)
	{
		if(tmptext != NULL)
		{
			time_t now;
			u32_t n = 0;

			message = (syslogmsg_t *)mem_malloc(sizeof(syslogmsg_t));
			if(message != NULL)
			{
				struct tm systime;

				/* Getting RTC clock. */
				RTC_TIME_Type ptime;
				RTC_GetFullTime(LPC_RTC, &ptime);

				/* Stupid Conversion. */
				systime.tm_hour = ptime.HOUR;
				systime.tm_min = ptime.MONTH;
				systime.tm_sec = ptime.SEC;
				systime.tm_mday = ptime.DOM;
				systime.tm_mon = ptime.MONTH;
				systime.tm_year = ptime.YEAR;
				now = mktime(&systime);

				/* TODO: Put complete syslog protocol here! Look: RFC 5424 */
				n = sprintf(tmptext, "<%d>%.15s %s", MYFACILITY | severity, ctime(&now) + 4, msg); /* Simple Format Message. */
				tmptext[n] = '\0'; 						// Add trailing EOF descriptor
				message->_next = NULL; 					// This is the last message (we know it)
				message->text = (char*)mem_malloc(n); // Allocate message
				memcpy(message->text, tmptext, n); 		// Set pointer of the allocated message
				mem_free(tmptext); 						// Free temprorary message space
				message->size = n; 					// Send message size
				if(curwp != NULL) 					// If this is not the first message in the queue
				{
					curwp->_next = message; 		// Set previous messages _next to this message
					curwp = message; 				// Set current message pointer to this message
					curwp->_next = NULL; 			// Set next to NULL to be sure
				}
				else
				{
					first = message; 				// Else this is the first message ever
					curwp = first; 					// Set curwp pointer to this message also
				}
			}
			else
			{
//				LWIP_DEBUGF(SYSLOG_DEBUG, "Cannot allocate memory for message.");
			}
		}
		else
		{
//			LWIP_DEBUGF(SYSLOG_DEBUG, "Cannot allocate memory for tmptext.");
		}
		vTaskResume(syslogHandle);
	}
}

/*-----------------------------------------------------------------------------------*/
static void syslogTask(void *pvParameters)
{
	syslogmsg_t *tmp; /* Temporary syslog message. */
	struct netconn *conn;
	struct ip4_addr addr;
	struct netbuf *buf;
	s8_t *data;

	first = NULL;
	curwp = NULL;

	while (1)
	{
		if(first != NULL)
		{
			do
			{
				tmp = first;
				if(first->text != NULL)
				{
					conn = netconn_new(NETCONN_UDP);
					if(conn != NULL)
					{
						ip4addr_aton(SYSLOGDEFSRVIP, &addr);
						if(netconn_connect(conn, &addr, SYSLOGDEFPORT) == ERR_OK)
						{
							buf = netbuf_new();
							data = netbuf_alloc(buf, first->size);
							memcpy (data, first->text, first->size);
							netconn_send(conn, buf);
							netbuf_delete(buf);
						}
						else
						{
//							LWIP_DEBUGF(SYSLOG_DEBUG, "syslog_thread: netconn connect error");
						}
						netconn_delete(conn);
					}
					else
					{
//						LWIP_DEBUGF(SYSLOG_DEBUG, "syslog_thread: netconn error");
					}
					if(tmp->text != NULL) mem_free(tmp->text);
				}
				first = first->_next;
				mem_free(tmp);

				if(first == NULL) curwp = NULL;

				vTaskDelay(configTICK_RATE_HZ/2);

			} while(tmp->_next != NULL);
		}
		vTaskSuspend(syslogHandle);
	}
}

/*-----------------------------------------------------------------------------------*/
sys_thread_t syslogGetHandle(void)
{
	return (syslogHandle);
}

/*-----------------------------------------------------------------------------------*/
void syslogSetHandle(sys_thread_t handle)
{
	syslogHandle = handle;
}

/*-----------------------------------------------------------------------------------*/
sys_thread_t syslogInit(void *syslogconfig, u16_t stacksize, u8_t taskprio)
{
	syslogHandle = sys_thread_new("syslogsrv", syslogTask, syslogconfig, stacksize, taskprio);
	return(syslogHandle);
}

/******************************************************************************
 *                    IMPLEMENTACAO DAS FUNCOES PRIVADAS					  *
 *****************************************************************************/

/******************************************************************************	
 *                                    EOF                                     *
 *****************************************************************************/
