#ifndef SSDP_SSDP_H_
#define SSDP_SSDP_H_

/** ************************************************************************
 * Modulo: ssdp 
 * @file ssdp.h
 * @headerfile ssdp.h
 * @author Marcelo Martins Maia do Couto - Email: marcelo.m.maia@gmail.com
 * @date Dec 5, 2015
 *
 * @brief Substitua este texto por uma descrição breve deste módulo.
 *
 * Substitua este texto pela descrição completa deste módulo.
 * Este módulo é um modelo para a criação de novos módulos. Ele contém
 * toda a estrutura que um módulo deve conter, sendo composto pelos arquivos:
 *   - ssdp.c;
 *   - ssdp.h.
 *
 * @copyright Copyright 2015 M3C Tecnologia
 * @copyright Todos os direitos reservados.
 *
 * @note
 *  - Não sobrescreva os arquivos de template do módulo. Implemente um novo
 *    módulo sobre uma cópia do template.
 *  - Os padrões de comentário que começam com "/** ", como este, devem ser
 *    compilados com a ferramenta Doxygen (comando:
 *    "doxygen.exe doxygen.cfg").
 *  - Leia a documentação do @b Doxygen para maiores informações sobre o
 *    funcionamento dos recursos de documentação de código.
 *
 * @warning
 *  - É altamente recomendado manter todos os arquivos de template como
 *    somente-leitura, evitando assim que eles sejam sobrescritos ao serem
 *    utilizados.
 *
 * @attention
 *  - A descrição de cada módulo como um todo deve ser realizada no arquivo
 *    ".h" do mesmo.
 *  - Cada módulo de um projeto de software deve conter, pelo menos, um
 *    arquivo ".h" e um ".c".
 * @pre 
 *   Coloque algum pré-requisito para utilizar este módulo aqui.
 *
 ******************************************************************************/

/*
 * Inclusão de arquivos de cabeçalho da ferramenta de desenvolvimento.
 * Por exemplo: '#include <stdlib.h>'.
 */

/*
 * Inclusão de arquivos de cabeçalho sem um arquivo ".c" correspondente.
 * Por exemplo: '#include "stddefs.h"'.
 */

/*
 * Inclusão de arquivos de cabeçalho de outros módulos utilizados por este.
 * Por exemplo: '#include "serial.h"'.
 */
#include <lwip/netif.h>
#include <lwip/err.h>

/*******************************************************************************
 *                           DEFINICOES E MACROS							   *
 ******************************************************************************/
#define UPNP_CACHE_SEC 			(UPNP_CACHE_SEC_MIN + 1)/* cache time we use */
#define UPNP_CACHE_SEC_MIN    	1800  					/* min cachable time per UPnP standard */
#define UPNP_RANDOM_TIME      	UPNP_CACHE_SEC / 4 + (((UPNP_CACHE_SEC / 4) * (rand() & 0xFFFFF)) >> 16)
#define UPNP_ADVERTISE_REPEAT 	2     					/* no more than 3 */

#define SSDP_UUID_DEVICE		"E5C7AF7A-E384-4B59-9E4E-5C7AF7A8E384"
#define SSDP_MAX_RESPONSE_SIZE	260

#define UPNP_MULTICAST_ADDRESS  "239.255.255.250" 		/* for UPnP multicasting */
#define UPNP_MULTICAST_PORT   	1900

/* */
#define RECEIVEDNOTIFY 			"NOTIFY *"
#define RECEIVEDSEARCH 			"M-SEARCH *"
#define RECEIVEDSSDPAL 			"ssdp:all"
#define RECEIVEDUUID			"uuid:"
#define RECEIVEDUPNPRO			"upnp:rootdevice"
#define RECEIVEDSSDPDI			"\"ssdp:discover\""
#define RECEIVEDHOST 			"HOST"
#define RECEIVEDST  			"ST"
#define RECEIVEDMAN				"MAN"
#define RECEIVEDMX				"MX"

#define TRANSMITTEDNOT 			"NOTIFY * HTTP/1.1\r\nHOST: %s:%d\r\nCACHE-CONTROL: max-age=%d\r\n"
#define TRANSMITTEDNTS 			"NTS: ssdp:%s\r\n"
#define TRANSMITTEDALI 			"alive"
#define TRANSMITTEDBYE 			"byebye"
#define TRANSMITTEDHTT 			"HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=%d\r\nEXT:\r\n"
#define TRANSMITTEDLOC 			"LOCATION: http://%s:%d/%s\r\nSERVER: unspecified, UPnP/1.0, unspecified\r\n"
#define TRANSMITTEDAD1 			"%s: upnp:rootdevice\r\nUSN: uuid:%s::upnp:rootdevice\r\n\r\n"
#define TRANSMITTEDAD2 			"%s: uuid:%s\r\nUSN: uuid:%s\r\n\r\n"

#define UPNPXMLENABLE			0
#define UUIDSTATIC				1

/*******************************************************************************
 *                     ESTRUTURAS E DEFINICOES DE TIPOS						   *	
 ******************************************************************************/
typedef enum
{
	ADVERTISE_UP = 0,
	ADVERTISE_DOWN,
	MSEARCH_REPLY
} ssdpstate_t;
/*******************************************************************************
 *                       VARIAVEIS PUBLICAS (Globais)						   *
 ******************************************************************************/

/*******************************************************************************
 *                      PROTOTIPOS DAS FUNCOES PUBLICAS						   *
 ******************************************************************************/
sys_thread_t ssdpServerInit(void *config, u16_t stacksize, u8_t taskprio);

/*******************************************************************************
 *                                   EOF									   *	
 ******************************************************************************/
#endif
