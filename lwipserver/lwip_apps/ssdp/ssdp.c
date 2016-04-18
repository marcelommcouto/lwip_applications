/** 
 *   Modulo: ssdp
 *   @file ssdp.c
 *   Veja ssdp.h para mais informações.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/*
 * Inclusão de arquivos de cabeçalho sem um arquivo ".c" correspondente.
 * Por exemplo: 
 * #include "stddefs.h" 
 * #include "template_header.h"
 */
#include "lwipopts.h"

/*
 * Inclusão de arquivos de cabeçalho de outros módulos utilizados por este.
 * Por exemplo: '#include "serial.h"'.
 */
#include "FreeRTOS.h"
#include "task.h"

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
#include "lwipopts.h"

/* UPNP WebServer to Reply XML Device Information. */
#include "upnp.h"

/*
 * Inclusão dos arquivos ".tab.h" deste módulo.
 * Por exemplo: 
 * #include "barcode.tab.h"
 * #include "template.tab.h"
 */

/*
 * Inclusão do arquivo de cabeçalho deste módulo.
 */
#include "ssdp.h"

/*******************************************************************************
 *                     CONSTANTES E DEFINICOES DE MACRO						   *
 ******************************************************************************/

/*******************************************************************************
 *                      ESTRUTURAS E DEFINIÇÕES DE TIPOS					   *
 ******************************************************************************/

/*******************************************************************************
 *                        VARIÁVEIS PUBLICAS (Globais)						   *
 ******************************************************************************/
static sys_thread_t ssdphandle = NULL;

/*******************************************************************************
 *                  DECLARACOES DE VARIAVEIS PRIVADAS (static)				   *
 ******************************************************************************/

/*******************************************************************************
 *                   PROTOTIPOS DAS FUNCOES PRIVADAS (static)				   *
 ******************************************************************************/
static void vSSDPServerTask(void *pvParameters);
static u16_t ssdpDataSplit(const u8_t *l);
static u16_t ssdpCheckToken(const u8_t *s1, const u8_t *s2);
static u8_t ssdpQtdeSpacesFound(const u8_t *s);
static u8_t ssdpLengthToken(const u8_t *s);
static s32_t ssdpCheckInitialString(const u8_t *str, const u8_t *start);
static s8_t ssdpConvertUUIDBintoStr(const u8_t *bin, u8_t *str, size_t max_len);
static void ssdpAdaptablePrintf(u8_t *buf, const u8_t *fmt, ...);
static void ssdpUUIDGenerator(u8_t *guid);

/*******************************************************************************
 *                      IMPLEMENTACAO DAS FUNCOES PUBLICAS					   *
 ******************************************************************************/
sys_thread_t ssdpServerInit(void *config, u16_t stacksize, u8_t taskprio)
{
	ssdphandle = sys_thread_new("SSDPServ", vSSDPServerTask, config, stacksize, taskprio);

	return(ssdphandle);
}

/******************************************************************************
 *                    IMPLEMENTACAO DAS FUNCOES PRIVADAS					  *
 *****************************************************************************/
static void vSSDPServerTask(void *pvParameters)
{
	struct netconn *conn;
	u8_t *response;
	u16_t len = 0;
	u16_t ssdpportorigin = 0;
	err_t err;
	ip_addr_t ssdpiporigin;
	ssdpstate_t ssdpState = ADVERTISE_UP;

	conn = netconn_new(NETCONN_UDP);
	if (conn != NULL)														/* Create a new UDP connection handle */
	{
#if LWIP_IGMP
		ip4_addr_t ipmultic;
		struct netif *iplocal;
		/* Using IGMP for Multicast data. */
		iplocal = (struct netif *)pvParameters;
		ipmultic.addr = ipaddr_addr(UPNP_MULTICAST_ADDRESS);
		err = netconn_join_leave_group(conn, &ipmultic, &iplocal->ip_addr, NETCONN_JOIN);
#endif
		if(err == ERR_OK)
		{
			err = netconn_bind(conn, IP_ADDR_ANY, UPNP_MULTICAST_PORT);		/* Bind to port 1900 with default IP address */
			if (err == ERR_OK)
			{
				struct netbuf *udpnetbuf;
				u8_t uuid_string[40];

#if UPNPXMLENABLE
				/* Initialize UPNP Fake Web. */
				upnpServerInit(pvParameters, (configMINIMAL_STACK_SIZE * 4), (tskIDLE_PRIORITY + 1));
#endif
				conn->recv_timeout = UPNP_RANDOM_TIME;												/* Set recv timeout. */
#if UUIDSTATIC
				memcpy(&uuid_string[0], (const char *)SSDP_UUID_DEVICE, sizeof(SSDP_UUID_DEVICE));	/* For Static UUID. */
#else
				ssdpUUIDGenerator(&uuid_string[0]);													/* For Dynamic UUID. */
#endif
				while(1)
				{
					err = netconn_recv(conn, &udpnetbuf);					/* Receive SSDP Client Commands. */
					if (err == ERR_OK)
					{
						err = netbuf_data(udpnetbuf, (void **) &response, &len);	/* Get pointer to response data. */
						if (err == ERR_OK)
						{
							if (strstr((const char *)response, (const char *)RECEIVEDNOTIFY)) 	/* Silently ignore NOTIFYs to avoid filling debug log. */
							{
								printf("N");
							}
							else
							if(strstr((const char *)response, (const char *)RECEIVEDSEARCH))
							{
								bool got_host = false, got_st = false, st_match = false, got_man = false, got_mx = false;
								u32_t ssdp_time_interval = 0;

								response += ssdpDataSplit(response);
								for (; *response != '\0'; response += ssdpDataSplit(response))	/* Parse remaining lines */
								{
									if (ssdpCheckToken(response, (const u8_t *)RECEIVEDHOST))
									{
										/* The host line indicates who the packet is addressed to... but do we really care? */
										got_host = true;
										continue;
									}
									else if (ssdpCheckToken(response, (const u8_t *)RECEIVEDST))
									{
										/* There are a number of forms; we look for one that matches our case. */
										got_st = true;

										response += ssdpLengthToken(response);
										response += ssdpQtdeSpacesFound(response);
										if (*response != ':') continue;
										response++;
										response += ssdpQtdeSpacesFound(response);
										if(strstr((const char *)response, (const char *)RECEIVEDSSDPAL))
										{
											st_match = true;
										}
										if(strstr((const char *)response, (const char *)RECEIVEDUPNPRO))
										{
											st_match = true;
										}
										if(strstr((const char *)response, (const char *)RECEIVEDUUID))
										{
											response += strlen(RECEIVEDUUID);
											ssdpConvertUUIDBintoStr((const u8_t *)SSDP_UUID_DEVICE, (u8_t *)&uuid_string, sizeof(uuid_string));
											if(strstr((const char *)response, (const char *)&uuid_string))
											{
												st_match = true;
											}
										}
										continue;
									}
									else if (ssdpCheckToken(response, (const u8_t *)RECEIVEDMAN))
									{
										response += ssdpLengthToken(response);
										response += ssdpQtdeSpacesFound(response);
										if (*response != ':') continue;
										response++;
										response += ssdpQtdeSpacesFound(response);
										if (ssdpCheckInitialString(response, (const u8_t *)RECEIVEDSSDPDI))
										{
											got_man = true;
										}
										continue;
									}
									else if (ssdpCheckToken(response, (const u8_t *)RECEIVEDMX))
									{
										response += ssdpLengthToken(response);
										response += ssdpQtdeSpacesFound(response);
										if (*response != ':') continue;
										response++;
										response += ssdpQtdeSpacesFound(response);
										ssdp_time_interval = atol((const char *)response);
										got_mx = true;
										continue;
									}
//									/* ignore anything else */
								}
								if (got_host && got_st && got_man && got_mx && st_match && (ssdp_time_interval >= 0))
								{
									if (ssdp_time_interval > 120) ssdp_time_interval = 120; /* UPnP-arch-DeviceArchitecture, 1.2.3 */
									ssdp_time_interval *= 2;
									ssdpState = MSEARCH_REPLY;
									ip_addr_set(&ssdpiporigin, &udpnetbuf->addr);
									ssdpportorigin = udpnetbuf->port;
//									printf("\nSearch OK! IP: %s:%d\n", ip4addr_ntoa((const ip_addr_t *)&ssdpiporigin), ssdpportorigin);
								}
							}
						}
					}
					else
					{
						/* Transmitting SSDP Client Commands. */
						if(iplocal->ip_addr.addr != 0)
						{
							u8_t *msgtosend;
							s8_t *NTString = "NT";

							/* Send data in multicast mode to anunciate my presence. */
						    msgtosend = mem_calloc(SSDP_MAX_RESPONSE_SIZE, sizeof(u8_t));

						    if (msgtosend != NULL)
						    {
							    switch (ssdpState)
							    {
									case ADVERTISE_UP:
									case ADVERTISE_DOWN:
									  ssdpAdaptablePrintf(msgtosend, (const u8_t *)TRANSMITTEDNOT, UPNP_MULTICAST_ADDRESS, UPNP_MULTICAST_PORT, UPNP_CACHE_SEC);
									  ssdpAdaptablePrintf(msgtosend, (const u8_t *)TRANSMITTEDNTS, (ssdpState == ADVERTISE_UP ? TRANSMITTEDALI : TRANSMITTEDBYE));
									break;
									case MSEARCH_REPLY:
						    			NTString = (s8_t *)"ST";
										ssdpAdaptablePrintf(msgtosend, (const u8_t *)TRANSMITTEDHTT, UPNP_CACHE_SEC);
									break;
							    }
							    if (ssdpState != ADVERTISE_DOWN)
							    {
							    	/* Where others may get our XML files from */
							    	ssdpAdaptablePrintf(msgtosend, (const u8_t *)TRANSMITTEDLOC, ip4addr_ntoa((const ip_addr_t *)&iplocal->ip_addr), UPNP_HTTP_PORT, UPNP_DEVICE_XML_FILE);
							    }

							    ssdpConvertUUIDBintoStr(SSDP_UUID_DEVICE, uuid_string, sizeof(uuid_string));

							    switch (ssdpState / UPNP_ADVERTISE_REPEAT)
							    {
							    	case 0:
							    		ssdpAdaptablePrintf(msgtosend, (const u8_t *)TRANSMITTEDAD1, NTString, uuid_string);
							    	break;
							    	case 1:
							    		ssdpAdaptablePrintf(msgtosend, (const u8_t *)TRANSMITTEDAD2, NTString, uuid_string, uuid_string);
							    	break;
							    }

							    udpnetbuf = netbuf_new();
							    if(udpnetbuf != NULL)
							    {
									if(netbuf_ref(udpnetbuf, msgtosend, SSDP_MAX_RESPONSE_SIZE) == ERR_OK)
									{
										netconn_sendto(conn, udpnetbuf, &ssdpiporigin, ssdpportorigin);
									}
							    }
							    mem_free(msgtosend);
							    if ((ssdpState + 1) >= 2 * UPNP_ADVERTISE_REPEAT)
							    {
							    	ssdpState = 0;
							    }
							    ssdpState = ADVERTISE_UP;
						    }
						}
					}
					netbuf_delete(udpnetbuf);
				}
			}
		}
	}
}

/*****************************************************************************/
static u16_t ssdpDataSplit(const u8_t *l)			/* No. of chars through (including) end of line */
{
	const u8_t *lp = l;

	while (*lp && *lp != '\n') lp++;
	if (*lp == '\n') lp++;

	return (lp - l);
}

/*****************************************************************************/
/* Check tokens for equality, where tokens consist of letters, digits, underscore and hyphen, and are matched case insensitive. */
static u16_t ssdpCheckToken(const u8_t *s1, const u8_t *s2)
{
	u16_t c1, c2, end1 = 0, end2 = 0;

	while(1)
	{
		c1 = *s1++;
		c2 = *s2++;
		if (isalpha(c1) && isupper(c1))	c1 = tolower(c1);
		if (isalpha(c2) && isupper(c2))	c2 = tolower(c2);
		end1 = !(isalnum(c1) || c1 == '_' || c1 == '-');
		end2 = !(isalnum(c2) || c2 == '_' || c2 == '-');
		if (end1 || end2 || c1 != c2) break;
	}
	return (end1 && end2); /* reached end of both words? */
}

/*****************************************************************************/
/* Return length of interword separation. This accepts only spaces/tabs and thus will not traverse a line or buffer ending. */
static u8_t ssdpQtdeSpacesFound(const u8_t *s)
{
	const u8_t *begin = s;

	for (;; s++)
	{
		const u8_t c = *s;
		if ((c == ' ') || (c == '\t')) continue;
		break;
	}
	return (s - begin);
}

/*****************************************************************************/
/* Return length of token (see above for definition of token) */
static u8_t ssdpLengthToken(const u8_t *s)
{
	const u8_t *begin = s;

	for (;; s++)
	{
		u8_t c = *s;
		s32_t end = !(isalnum(c) || c == '_' || c == '-');

		if (end) break;
	}
	return (s - begin);
}

/*****************************************************************************/
static s32_t ssdpCheckInitialString(const u8_t *str, const u8_t *start)
{
	return (strncmp((const char *)str, (const char *)start, strlen((const char *)start)) == 0);
}

/*****************************************************************************/
static s8_t ssdpConvertUUIDBintoStr(const u8_t *bin, u8_t *str, size_t max_len)
{
	u16_t len;

	len = snprintf((char *)str, max_len, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	bin[0], bin[1], bin[2], bin[3],	bin[4], bin[5], bin[6], bin[7],
	bin[8], bin[9], bin[10], bin[11], bin[12], bin[13], bin[14], bin[15]);
	if (len < 0 || (size_t) len >= max_len) return -1;
	return 0;
}

/*****************************************************************************/
static void ssdpAdaptablePrintf(u8_t *buf, const u8_t *fmt, ...)
{
	va_list ap;
	s8_t *tmp = buf + strlen(buf);

	va_start(ap, fmt);
	vsprintf(tmp, fmt, ap);
	va_end(ap);
}

/*****************************************************************************/
static void ssdpUUIDGenerator(u8_t *guid)
{
	u16_t t = 0;
	s8_t *szTemp = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
	s8_t *szHex = "0123456789ABCDEF-";
	u16_t nLen = strlen (szTemp);

	for (t = 0; t < (nLen + 1); t++)
	{
		int r = rand () % 16;
		char c = ' ';

		switch (szTemp[t])
		{
			case 'x' : { c = szHex [r]; } break;
			case 'y' : { c = szHex [(r & 0x03) | 0x08]; } break;
			case '-' : { c = '-'; } break;
			case '4' : { c = '4'; } break;
		}
		guid[t] = ( t < nLen ) ? c : 0x00;
	}
//	printf ("\nUUID: %s\n", guid);
}

/******************************************************************************	
 *                                    EOF                                     *
 *****************************************************************************/
