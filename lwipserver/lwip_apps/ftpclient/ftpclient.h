#ifndef _FTPCLIENT_H_
#define _FTPCLIENT_H_

/** ************************************************************************
 * Modulo: ftpclient 
 * @file ftpclient.h
 * @headerfile ftpclient.h
 * @author Marcelo Martins Maia do Couto - Email: marcelo.m.maia@gmail.com
 * @date Jul 3, 2015
 *
 * @brief Substitua este texto por uma descrição breve deste módulo.
 *
 * Substitua este texto pela descrição completa deste módulo.
 * Este módulo é um modelo para a criação de novos módulos. Ele contém
 * toda a estrutura que um módulo deve conter, sendo composto pelos arquivos:
 *   - ftpclient.c;
 *   - ftpclient.h.
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
#include <string.h>
#include <stdio.h>

/*
 * Inclusão de arquivos de cabeçalho sem um arquivo ".c" correspondente.
 * Por exemplo: '#include "stddefs.h"'.
 */
#include "LPC17xx.h"

/*
 * Inclusão de arquivos de cabeçalho de outros módulos utilizados por este.
 * Por exemplo: '#include "serial.h"'.
 */
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/api.h"
#include "lwip/stats.h"

/*******************************************************************************
 *                           DEFINICOES E MACROS							   *
 ******************************************************************************/
/* Error codes from eCos*/
// to do: harmonize with lwip's error codes

#define FTP_BAD				(-2) /* Catch all, socket errors etc. */
#define FTP_NOSUCHHOST		(-3) /* The server does not exist. */
#define FTP_BADUSER			(-4) /* Username/Password failed */
#define FTP_TOOBIG			(-5) /* Out of buffer space or disk space */
#define FTP_BADFILENAME		(-6) /* The file does not exist */
#define FTP_BAD_ARGUMENT	(-7)
#define FTP_TIMEDOUT		(-8)

//#define FTPC_GETLINE_TIMEOUT_ENABLE			// enable to protect from long input messages
#define FTPC_GETLINE_TIMEOUT		10000		// timeout for messages in ms

/* Enable/disable debug output for FTP Client application. */
#define FTPC_DEBUG					printf
#define FTPC_ERROR					FTPC_DEBUG
#define FTPC_MSG_BUFF 				60	// answer to PASV usually is the longest string

/* Some definitions. */
#define FTPSERVERPORTC				21

/*******************************************************************************
 *                     ESTRUTURAS E DEFINICOES DE TIPOS						   *	
 ******************************************************************************/


/*******************************************************************************
 *                       VARIAVEIS PUBLICAS (Globais)						   *
 ******************************************************************************/
u16_t last_file_size;
volatile u32_t TimeoutFTPC;

/*******************************************************************************
 *                      PROTOTIPOS DAS FUNCOES PUBLICAS						   *
 ******************************************************************************/

/**
 * Inicialização deste módulo.
 *
 * @return sys_thread_t
 *
 * @pre Descreva a pré-condição para esta função
 *
 * @post Descreva a pós-condição para esta função
 *
 * @invariant Descreva o que não pode variar quando acabar a execução da função 
 *
 * @note
 *  Esta função deve ser chamada durante a inicialização do software caso este
 *  módulo seja utilizado.
 *
 * <b> Exemplo de uso: </b>
 * @code
 *    ftpclientInit(NULL, 256, 2);
 * @endcode
 ******************************************************************************/
sys_thread_t ftpclientInit(void *ftpclientconfig, u16_t stacksize, u8_t taskprio);

//int ftp_get(char *hostname, char *username, char *passwd, char *filename, char *buf, u16_t buf_size, u8_t where);
//int ftp_put(char * hostname, char * username, char * passwd, char * filename, char * buf, u16_t len);
//int ftp_upload(char* src, u16_t len);

err_t ftp_put(u8_t *hostname, u8_t *username, u8_t *passwd, const u8_t *filename, u8_t *buf, u16_t len);
err_t ftp_get(u8_t *hostname, u8_t *username, u8_t *passwd, const u8_t *filename, u8_t *buf, u16_t len, u8_t path);

/*******************************************************************************
 *                                   EOF									   *	
 ******************************************************************************/
#endif
