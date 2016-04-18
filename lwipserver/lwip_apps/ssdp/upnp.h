#ifndef UPNP_H_
#define UPNP_H_

/** ************************************************************************
 * Modulo: upnpweb 
 * @file upnpweb.h
 * @headerfile upnpweb.h
 * @author Marcelo Martins Maia do Couto - Email: marcelo.m.maia@gmail.com
 * @date Dec 7, 2015
 *
 * @brief Substitua este texto por uma descrição breve deste módulo.
 *
 * Substitua este texto pela descrição completa deste módulo.
 * Este módulo é um modelo para a criação de novos módulos. Ele contém
 * toda a estrutura que um módulo deve conter, sendo composto pelos arquivos:
 *   - upnpweb.c;
 *   - upnpweb.h.
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
#include "ssdp.h"

/*******************************************************************************
 *                           DEFINICOES E MACROS							   *
 ******************************************************************************/
#define UPNP_DEVICE_XML_FILE 	"device.xml"
#define UPNP_HTTP_PORT        	5200
#define UPNP_SERVER_ONLINE		5000
#define UPNP_HTTP_CMD_VALID		"GET /"

/* UPNP Device Details. */
#define UPNP_FRIENDLY_NAME      "MyGuardian"
#define UPNP_MANUFACTURER       "Marcelo Couto"
#define UPNP_MANUFACTURER_URL   "http://marcelommcouto.wordpress.com"
#define UPNP_MODEL_DECRIPTION   "XM700"
#define UPNP_MODEL_NAME         "LPC1768"
#define UPNP_MODEL_NUMBER       "1.0"
#define UPNP_MODEL_URL          "http://marcelommcouto.wordpress.com"
#define UPNP_UPC                ""
#define UPNP_URL_BASE           ""
#define UPNP_SERIAL_NUMBER		""

#define UPNP_HTTP_HEADER		"HTTP/1.1 200 OK\r\nMIME-Version: 1.0\r\nServer: unspecified, UPnP/1.0, unspecified\r\nConnection: close\r\nContent-Type: text/xml;charset=utf-8\r\nContent-Length: %d\r\nAccept-Ranges: none\r\n\r\n"

#define UPNP_DEVICE_XML_PRE		"<?xml version=\"1.0\"?>\n<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n<specVersion>\n<major>1</major>\n<minor>0</minor>\n</specVersion>\n"
#define UPNP_DEVICE_XML_MID     "<URLBase>%s</URLBase>\n<device>\n<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>\n"
#define UPNP_DEVICE_XML_MYDATA  "<friendlyName>%s</friendlyName>\n<manufacturer>%s</manufacturer>\n<manufacturerURL>%s</manufacturerURL>\n<modelDescription>%s</modelDescription>\n<modelName>%s</modelName>\n<modelNumber>%s</modelNumber>\n<modelURL>%s</modelURL>\n<serialNumber>%s</serialNumber>\n<UDN>uuid:%s</UDN>\n<UPC>%s</UPC>\n"
#define UPNP_DEVICE_XML_POS		"<serviceList>\n<service>\n<serviceType>urn:schemas-upnp-org:service:Basic:1</serviceType>\n<serviceId>urn:upnp-org:serviceId:1</serviceId>\n<SCPDURL>/%s</SCPDURL>\n<controlURL></controlURL>\n<eventSubURL></eventSubURL>\n</service>\n</serviceList>\n"
#define UPNP_DEVICE_XML_SPOS	"</device>\n</root>\n"

/*******************************************************************************
 *                     ESTRUTURAS E DEFINICOES DE TIPOS						   *	
 ******************************************************************************/

/*******************************************************************************
 *                       VARIAVEIS PUBLICAS (Globais)						   *
 ******************************************************************************/

/*******************************************************************************
 *                      PROTOTIPOS DAS FUNCOES PUBLICAS						   *
 ******************************************************************************/
sys_thread_t upnpServerInit(void *config, u16_t stacksize, u8_t taskprio);

/*******************************************************************************
 *                                   EOF									   *	
 ******************************************************************************/
#endif
