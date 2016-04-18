#ifndef _SYSLOG_H_
#define _SYSLOG_H_

/** ************************************************************************
 * Modulo: syslog 
 * @file syslog.h
 * @headerfile syslog.h
 * @author Marcelo Martins Maia do Couto - Email: marcelo.m.maia@gmail.com
 * @date Jul 1, 2015
 *
 * @brief Substitua este texto por uma descrição breve deste módulo.
 *
 * Substitua este texto pela descrição completa deste módulo.
 * Este módulo é um modelo para a criação de novos módulos. Ele contém
 * toda a estrutura que um módulo deve conter, sendo composto pelos arquivos:
 *   - syslog.c;
 *   - syslog.h.
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
#include <stdio.h>
#include <string.h>
/*
 * Inclusão de arquivos de cabeçalho sem um arquivo ".c" correspondente.
 * Por exemplo: '#include "stddefs.h"'.
 */
#include "time.h"
/*
 * Inclusão de arquivos de cabeçalho de outros módulos utilizados por este.
 * Por exemplo: '#include "serial.h"'.
 */
#include "FreeRTOS.h"
#include "task.h"

/*******************************************************************************
 *                           DEFINICOES E MACROS							   *
 ******************************************************************************/
typedef struct syslogmsg
{
    s8_t * text;
    u32_t size;
    struct syslogmsg *_next;
} syslogmsg_t;

/* Syslog Message Facilities. */
#define LOG_KERNEL		(0 << 3)	/* Kernel messages. */
#define LOG_USER		(1 << 3)	/* User-level messages. */
#define LOG_MAIL		(2 << 3)	/* Mail system. */
#define LOG_SYSTEM		(3 << 3)	/* System daemons. */
#define LOG_SECURITY	(4 << 3)	/* Security/authorization messages. */
#define LOG_INTERNAL	(5 << 3)	/* Messages generated internally by syslogd. */
#define LOG_PRINTER		(6 << 3)	/* Line printer subsystem. */
#define LOG_NNEWS		(7 << 3)	/* Network news subsystem. */
#define LOG_UUCP		(8 << 3)	/* UUCP subsystem. */
#define LOG_CLOCK		(9 << 3)	/* Clock daemon. */
#define LOG_AUTH		(10 << 3)	/* Security/authorization messages. */
#define LOG_FTP			(11 << 3)	/* FTP daemon. */
#define LOG_NTP			(12 << 3)	/* NTP subsystem. */
#define LOG_AUDIT		(13 << 3)	/* Log audit. */
#define LOG_ALERT		(14 << 3)	/* Log alert. */
#define LOG_CLOCK2		(15 << 3)	/* Clock daemon (note 2). */
#define LOG_LOCAL0      (16 << 3) 	/* Your uC will send messages with this ID */
#define LOG_LOCAL1      (17 << 3) 	/* reserved for local use */
#define LOG_LOCAL2      (18 << 3) 	/* reserved for local use */
#define LOG_LOCAL3      (19 << 3) 	/* reserved for local use */
#define LOG_LOCAL4      (20 << 3) 	/* reserved for local use */
#define LOG_LOCAL5      (21 << 3) 	/* reserved for local use */
#define LOG_LOCAL6      (22 << 3) 	/* reserved for local use */
#define LOG_LOCAL7      (23 << 3) 	/* reserved for local use */

/* Syslog Message Severities. */
#define SEVER_EMERG		0	/* Emergency: system is unusable; */
#define SEVER_ALERT     1   /* Alert: action must be taken immediately. */
#define SEVER_CRIT      2   /* Critical: critical conditions. */
#define SEVER_ERROR     3   /* Error: error conditions. */
#define SEVER_WARN      4   /* Warning: warning conditions. */
#define SEVER_NOTICE    5   /* Notice: normal but significant condition. */
#define SEVER_INFO      6   /* Informational: informational messages. */
#define SEVER_DEBUG     7   /* Debug: debug-level messages. */

#define MYFACILITY		LOG_LOCAL0
#define MESSAGESIZE		128
#define SYSLOGDEFPORT	514
#define SYSLOGDEFSRVIP	"192.168.2.50"

/*******************************************************************************
 *                     ESTRUTURAS E DEFINICOES DE TIPOS						   *	
 ******************************************************************************/

/*******************************************************************************
 *                       VARIAVEIS PUBLICAS (Globais)						   *
 ******************************************************************************/

/*******************************************************************************
 *                      PROTOTIPOS DAS FUNCOES PUBLICAS						   *
 ******************************************************************************/

/**
 * Inicialização deste módulo.
 *
 * @return void
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
 *    template_init();
 * @endcode
 ******************************************************************************/
sys_thread_t syslogInit(void *syslogconfig, u16_t stacksize, u8_t taskprio);

void syslogSend(const s8_t *msg, u8_t severity);
sys_thread_t syslogGetHandle(void);
void syslogSetHandle(sys_thread_t handle);

/*******************************************************************************
 *                                   EOF									   *	
 ******************************************************************************/
#endif
