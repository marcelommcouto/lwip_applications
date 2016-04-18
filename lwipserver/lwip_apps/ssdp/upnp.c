/** 
 *   Modulo: upnp
 *   @file upnp.c
 *   Veja upnp.h para mais informações.
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
#include <stdio.h>		/* Para definições de sprintf. */
#include "ctype.h"

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

/*
 * Inclusão dos arquivos ".tab.h" deste módulo.
 * Por exemplo: 
 * #include "barcode.tab.h"
 * #include "template.tab.h"
 */

/*
 * Inclusão do arquivo de cabeçalho deste módulo.
 */
#include "upnp.h"

/*******************************************************************************
 *                     CONSTANTES E DEFINICOES DE MACRO						   *
 ******************************************************************************/

/*******************************************************************************
 *                      ESTRUTURAS E DEFINIÇÕES DE TIPOS					   *
 ******************************************************************************/

/*******************************************************************************
 *                        VARIÁVEIS PUBLICAS (Globais)						   *
 ******************************************************************************/

/*******************************************************************************
 *                  DECLARACOES DE VARIAVEIS PRIVADAS (static)				   *
 ******************************************************************************/

/*******************************************************************************
 *                   PROTOTIPOS DAS FUNCOES PRIVADAS (static)				   *
 ******************************************************************************/
static void upnpWebServerTask(void *arg);
static void upnpWebServerUpdate(struct netconn *conn);

/*******************************************************************************
 *                      IMPLEMENTACAO DAS FUNCOES PUBLICAS					   *
 ******************************************************************************/
sys_thread_t upnpServerInit(void *config, u16_t stacksize, u8_t taskprio)
{
	sys_thread_new("UPNPServ", upnpWebServerTask, config, stacksize, taskprio);
}

/*****************************************************************************/
static void upnpWebServerTask(void *arg)
{
	struct netconn *conn, *newconn;
	err_t err;

	LWIP_UNUSED_ARG(arg);

	conn = netconn_new(NETCONN_TCP);							/* Create a new TCP connection handle */
	if (conn != NULL)
	{
		conn->recv_timeout = UPNP_SERVER_ONLINE;
		err = netconn_bind(conn, IP_ADDR_ANY, UPNP_HTTP_PORT);	/* Bind to port 5200 with default IP address */
		if (err == ERR_OK)
		{
			err = netconn_listen(conn);							/* Put the connection into LISTEN state */
			if(err == ERR_OK)
			{
				while(1)
				{
					err = netconn_accept(conn, &newconn);		/* accept any incoming connection */
					if(newconn)
					{
						newconn->recv_timeout = 500;
						upnpWebServerUpdate(newconn);			/* serve connection */
						netconn_delete(newconn);				/* delete connection */
					}
				}
			}
		}
	}
}

/*****************************************************************************/
static void upnpWebServerUpdate(struct netconn *conn)
{
	struct netbuf *inbuf;
	char *buf;
	u16_t buflen;

	/* Read the data from the port, blocking if nothing yet there.
	 * We assume the request (the part we care about) is in one netbuf */
	if(netconn_recv(conn, &inbuf) == ERR_OK)
	{
		if(netbuf_data(inbuf, (void **) &buf, &buflen) == ERR_OK) /* Get pointer to response data. */
		{
			char *pos = NULL;
			pos = strstr((const char *)buf, (const char *)UPNP_HTTP_CMD_VALID);
			if(pos != NULL)	/* GET Found! */
			{
				/* Getting ACCEPT-LANGUAGE. */
				buf += (sizeof(UPNP_HTTP_CMD_VALID) - 1);	/*/0 is incontable? */
				pos = strstr((const char *)buf, (const char *)UPNP_DEVICE_XML_FILE);
				if(pos != NULL)	/* UPNP_DEVICE_XML_FILE Found! */
				{
					/* OK! Let's response... */
					u8_t length_pre = 0;
					uint16_t length_mid = 0;
					uint16_t length_mydata = 0;
					uint16_t length_pos = 0;
					u8_t length_end = 0;
					char *xml_buf = NULL;

					/* First... Let's mount the xml file and get your size. */
					/* Static Header. Defined. */
					length_pre = strlen(UPNP_DEVICE_XML_PRE);
					/* Dynamic Body because UPNP_URL_BASE size. -2 because %s is changed. */
					length_mid = (strlen(UPNP_DEVICE_XML_MID) + strlen(UPNP_URL_BASE) - 2);
					/* Dynamic Body because UPNP_FRIENDLY_NAME and the others strings. - 20 because we have 10 %s */
					length_mydata += strlen(UPNP_FRIENDLY_NAME);
					length_mydata += strlen(UPNP_MANUFACTURER);
					length_mydata += strlen(UPNP_MANUFACTURER_URL);
					length_mydata += strlen(UPNP_MODEL_DECRIPTION);
					length_mydata += strlen(UPNP_MODEL_NAME);
					length_mydata += strlen(UPNP_MODEL_NUMBER);
					length_mydata += strlen(UPNP_MODEL_URL);
					length_mydata += strlen(UPNP_SERIAL_NUMBER);
					length_mydata += strlen(SSDP_UUID_DEVICE);  /* For reference... UUID size. */
					length_mydata += strlen(UPNP_UPC);
					length_mydata += (strlen(UPNP_DEVICE_XML_MYDATA) - 20);
					/* Dynamic Body because UPNP_DEVICE_XML_FILE size. -2 because %s is changed. */
					length_pos += strlen(UPNP_DEVICE_XML_FILE);
					length_pos += (strlen(UPNP_DEVICE_XML_POS) - 2);
					/* Static Header. Defined. */
					length_end = strlen(UPNP_DEVICE_XML_SPOS);

					/* Send HTML Header with XML file size. */
					xml_buf = mem_malloc((sizeof(UPNP_HTTP_HEADER) + 1));
					if(!xml_buf) return;
					sprintf(xml_buf, UPNP_HTTP_HEADER, (length_pre + length_mid + length_mydata + length_pos + length_end + 1));	/* +1 because %d (2) but number is xxx (3). */
					netconn_write(conn,(const char *)xml_buf, strlen(xml_buf), NETCONN_COPY);
					mem_free(xml_buf);

					/* Send First Piece from XML File. */
					xml_buf = mem_malloc(length_pre);
					if(!xml_buf) return;
					sprintf(xml_buf, UPNP_DEVICE_XML_PRE, length_pre);
					netconn_write(conn,(const char *)xml_buf, length_pre, NETCONN_COPY);
					mem_free(xml_buf);

					/* Send Second Piece from XML File. */
					xml_buf = mem_malloc(length_mid);
					if(!xml_buf) return;
					sprintf(xml_buf, UPNP_DEVICE_XML_MID, UPNP_URL_BASE);
					netconn_write(conn,(const char *)xml_buf, length_mid, NETCONN_COPY);
					mem_free(xml_buf);

					/* Send Third Piece from XML File. */
					xml_buf = mem_malloc(length_mydata);
					if(!xml_buf) return;
					sprintf(xml_buf, UPNP_DEVICE_XML_MYDATA, UPNP_FRIENDLY_NAME, UPNP_MANUFACTURER, UPNP_MANUFACTURER_URL,
									 UPNP_MODEL_DECRIPTION, UPNP_MODEL_NAME, UPNP_MODEL_NUMBER, UPNP_MODEL_URL,
									 UPNP_SERIAL_NUMBER, SSDP_UUID_DEVICE, UPNP_UPC);
					netconn_write(conn,(const char *)xml_buf, length_mydata, NETCONN_COPY);
					mem_free(xml_buf);

					/* Send Forth Piece from XML File. */
					xml_buf = mem_malloc(length_pos);
					if(!xml_buf) return;
					sprintf(xml_buf, UPNP_DEVICE_XML_POS, UPNP_DEVICE_XML_FILE);
					netconn_write(conn,(const char *)xml_buf, length_pos, NETCONN_COPY);
					mem_free(xml_buf);

					/* Send Last Piece from XML File. */
					xml_buf = mem_malloc(length_end);
					if(!xml_buf) return;
					sprintf(xml_buf, UPNP_DEVICE_XML_SPOS);
					netconn_write(conn,(const char *)xml_buf, length_end, NETCONN_COPY);
					mem_free(xml_buf);
				}
			}
		}
	}
	netbuf_delete(inbuf);	/* Delete the buffer and deallocate the buffer) */
	netconn_close(conn);	/* Close the connection (server closes in HTTP) */
}

/******************************************************************************
 *                    IMPLEMENTACAO DAS FUNCOES PRIVADAS					  *
 *****************************************************************************/

/******************************************************************************	
 *                                    EOF                                     *
 *****************************************************************************/
