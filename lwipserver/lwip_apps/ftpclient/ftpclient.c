/** 
 *   Modulo: ftpclient
 *   @file ftpclient.c
 *   Veja ftpclient.h para mais informações.
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
#include "lwip/mem.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/api.h"
#include "lwip/stats.h"

/*
 * Inclusão dos arquivos ".tab.h" deste módulo.
 * Por exemplo: 
 * #include "barcode.tab.h"
 * #include "template.tab.h"
 */

/*
 * Inclusão do arquivo de cabeçalho deste módulo.
 */
#include "ftpclient.h"

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
static sys_thread_t ftpclienthandle;

/*******************************************************************************
 *                   PROTOTIPOS DAS FUNCOES PRIVADAS (static)				   *
 ******************************************************************************/
static void ftpclientThread(void *arg);
static s8_t get_line(struct netconn *conn, uint8_t *buf, u16_t buf_size);
static s32_t get_reply(struct netconn *conn, u8_t *reply, u16_t reply_size);
static s32_t send_cmd(struct netconn *conn, const u8_t *str);
static s32_t send_data(struct netconn *conn, const u8_t *buf, u16_t bufsize);
static s32_t receive_data(struct netconn *conn, u8_t *buf, u16_t buf_size, u16_t *bytes);
static s16_t send_file(struct netconn *dataconn, const u8_t *buf, u16_t bufsize);
static s8_t ftpclientReceivedData(struct netconn *dataconn, u8_t *buf, u16_t len, u16_t *bytes);
static s8_t command(const u8_t *cmd, const u8_t *arg, struct netconn *conn, u8_t *buf, u16_t len);
static s8_t ftpclientOpenSession(struct netconn *ctrlconn, u8_t *username, u8_t *passwd, u8_t *buf, u16_t len);
static s8_t ftpclientCloseSession(struct netconn *ctrlconn, u8_t *buf, u16_t len);
static s8_t ftpclientOpenDataSession(struct netconn *ctrlconn, struct netconn *dataconn, u8_t *buf, u16_t len);
static s8_t login(u8_t *username, u8_t *passwd, struct netconn *conn, u8_t *buf, u16_t len);
static u16_t GetDynPort(u8_t* msg, u8_t* ip_str, u32_t len);
//static u16_t ftpclientGetFileSize(const u8_t *filename);

/*******************************************************************************
 *                      IMPLEMENTACAO DAS FUNCOES PUBLICAS					   *
 ******************************************************************************/
/*
 * A documentação destas funções é realizada no arquivo ".h" deste módulo.
 */

////////////////////////////////////////////////////////////
//  Open a data socket. User should be already logged in
//  input:
//		socket file descriptor of aleady established control connection
//		pointer to existing and initialized sockaddr_in struct for the data connection (local)
//		pointer to existing and initialized sockaddr_in struct for the data connection (remote)
//		message buffer and its length, used for temporary storage of server responses
//	returns:
//		sockfd of data connection if succesfull. Upper-layer app must release sockfd when finished or on error
//		<0 if failed
static s8_t ftpclientOpenDataSession(struct netconn *ctrlconn, struct netconn *dataconn, u8_t *buf, u16_t len)
{
	s8_t stat = ERR_OK;
	u16_t dynport = (FTPSERVERPORTC + 1); 			// server-side accept port, it's dynamic!
	u8_t ip[15];
	ip4_addr_t iplwipformat;

	stat = command("PASV", NULL, ctrlconn, buf, len); /* Response should be "227 Entering passive mode <IPADDR3,IPADDR2,IPADDR1,IPADDR0,0,20>". */

	if (stat < 0) return (stat);

	if (stat != 2)
	{
		FTPC_ERROR("PASV Command Failed!");
		return (FTP_BAD);
	}

	// calculate remote port waiting to be connected
	dynport = GetDynPort(buf, ip, len);	// NULL -> ignore remote IP string

	if (dynport == 0)
	{
    	FTPC_ERROR("Could Not Parse PASV Answer.");
		return (FTP_BAD);
	}

	ip4addr_aton(ip, &iplwipformat);
	if(netconn_connect(dataconn, &iplwipformat, dynport) != ERR_OK)
	{
		FTPC_DEBUG("\nNot Establish Data Conn.");
		return (FTP_BAD);
	}
	else
	{
		FTPC_DEBUG("\nData Conn. Established.");
	}

	return (ERR_OK);
}

/* ftpOpenCtrlSession - Connect to remote host, login, create control connection. */
static s8_t ftpclientOpenSession(struct netconn *ctrlconn, u8_t *username, u8_t *passwd, u8_t *buf, u16_t len)
{
	s8_t ret = ERR_OK;
	u8_t retry_cnt = 3;

	/* Read the welcome message from the server */
	ret = get_reply(ctrlconn, buf, len);	// should be 2 (OK), FTP_BAD or FTP_TIMEDOUT

	if (ret != 2)
	{	// sometimes (due to bugs in socket's API) it can happen that the input buffer
		// contains only the beginning of the message and get_line() is stuck on waiting
		// CR/LF. If timeout handling is enabled, this piece of code solves the problem

		if (ret == FTP_TIMEDOUT)
		{
			if (!strstr(buf, "220"))	// try to recover message
			{
				FTPC_DEBUG("\nFTPC: Welcome message not received (even with timeout).");
				return(FTP_TIMEDOUT);
			}
		}
		else
		{
			FTPC_DEBUG("\nFTPC: Server refused connection.");
			return(FTP_BAD);
		}
	}

	do
	{
		ret = login(username, passwd, ctrlconn, buf, len);
		if (ret != 2)					// "230 Logged on<CR><LF>" expected
		{
			if (strstr(buf, "331"))		// "331 Password required for 'user'<CR><LF>"
			{
				// This happens when the last part of the Welcome message comes after
				// its timeout expires (FTPC_GETLINE_TIMEOUT).
				// At this point server has received the USER command and expects password
				retry_cnt--;
			}
			return(FTP_BAD);
			FTPC_DEBUG("\nFTPC: Login failed!");
		}
		else
		{
			retry_cnt = 0;
		}
	} while (retry_cnt);

	FTPC_DEBUG("\nFTPC: Login successfull, server expects commands.");

	return (ERR_OK);
}

/* All done, quit */
static s8_t ftpclientCloseSession(struct netconn *ctrlconn, u8_t *buf, u16_t len)
{
	s8_t ret;

	ret = command("QUIT", NULL, ctrlconn, buf, len);

	if (ret != 2)
	{
        if (ret < 0)
		{
			FTPC_DEBUG("\nFTPC: Quit failed! Nothing Returned.");
			return (ret);
		}
        FTPC_DEBUG("\nFTPC: Quit failed!");
		return (FTP_BAD);
	}

	FTPC_DEBUG("\nFTPC: QUIT command OK.");
	return (ERR_OK);
}

///////////////////////////////////////////////////////////////////////////////
/* Get a file from an FTP server.
   "filename" should be the full pathname of the file. buf is a pointer to a buffer the
   contents of the file should be placed in and buf_size is the size of the buffer.

   Is this right? If the file is bigger than the buffer, buf_size bytes will be retrieved and an error code returned.
   On success the number of bytes received is returned. On error a negative value is returned indicating the type of error. */
err_t ftp_get(u8_t *hostname, u8_t *username, u8_t *passwd, const u8_t *filename, u8_t *buf, u16_t len, u8_t path)
{
	struct netconn *ftpclientc = NULL;
	struct netconn *ftpclientd = NULL;
	u8_t controlbuf[FTPC_MSG_BUFF];		/* Buffer to exchange commands with server. */
	err_t stats;
	ip4_addr_t srvip;

	ftpclientc = netconn_new(NETCONN_TCP);	/* Open Netcomm to FTP Control Comm. */
	if(ftpclientc == NULL)
	{
		FTPC_ERROR("\nFTPC: Error in Open FTP Control Netconn!");
		return(FTP_BAD);
	}

	ip4addr_aton(hostname, &srvip);	/* Getting IP. */
	stats = netconn_connect(ftpclientc, &srvip, FTPSERVERPORTC);	/* Trying to connect to server. */
	if(stats == ERR_OK)
	{
		stats = ftpclientOpenSession(ftpclientc, username, passwd, controlbuf, sizeof(controlbuf));	/* Opening a FTP Session. */
		if (stats != ERR_OK)
		{
			return (stats);
		}

		ftpclientd = netconn_new(NETCONN_TCP);	/* Open Netcomm to FTP Data Comm. */
		if(ftpclientd == NULL)
		{
			/* ERROR! Close actual session. */
			stats = ftpclientCloseSession(ftpclientc, controlbuf, sizeof(controlbuf));
			FTPC_ERROR("\nFTPC: Error in Open FTP Data Netconn!");
			return(FTP_BAD);
		}

		/* We are now logged in and ready to transfer the file. Open the data socket
		ready to receive the file. It also builds the PASV command ready to send */
		stats = ftpclientOpenDataSession(ftpclientc, ftpclientd, controlbuf, sizeof(controlbuf));
		if (stats != ERR_OK)
		{
			return (stats);
		}

		/* Ask the file size. */
		stats = command("SIZE", filename, ftpclientc, controlbuf, sizeof(controlbuf));

		/* Ask for the file */
		stats = command("RETR", filename, ftpclientc, controlbuf, sizeof(controlbuf));
		if (stats < 0)
		{
			return (stats);
		}

		if (stats != 1)
		{
			FTPC_ERROR("\nFTPC: RETR failed!");
			return (FTP_BADFILENAME);
		}

		u16_t bytes;

		stats = ftpclientReceivedData(ftpclientd, buf, len, &bytes);

	//	switch (where)			// destination: Flash or RAM buffer
	//	{
	//		case DEST_RAM:
	//		{
	//        	if ((bytes = receive_file_RAM(data_s, buf, bufsize)) < 0) // Receive the file into the buffer and close the data socket afterwards
	//			{
	//				FTPC_ERROR("FTPC: Receiving file failed (RAM)\n");
	//				netconn_close(ftpdatac);
	//				return (bytes);
	//			}
	//		} break;
	//		case DEST_FLASH:
	//		{
	//        	if ((bytes = receive_file_MBF(data_s, buf)) < 0)
	//			{
	//				FTPC_ERROR("FTPC: Receiving file failed (MB FLASH)\n");
	//				cb_close(data_s, 0);
	//				cb_close(ctrl_s, 0);
	//				return (bytes);
	//			}
	//
	//		}
	//		break;
	//		default:	// undefined destination
	//		{
	//       		FTPC_ERROR("\nFTPC: ftp_get() bug: undefined destination!\n");
	//    		netconn_close(ftpdatac);
	//			return (FTP_BAD_ARGUMENT);
	//		}
	//	}
		stats = ftpclientCloseSession(ftpclientc, controlbuf, sizeof(controlbuf));
	}
	return (stats);
}

///////////////////////////////////////////////////////////////////////////////
// ftp_put() - connects to server, login, open/create remote file <"filename">,
// send <buf_size> bytes from <buf>, log out, close data and control connections.
// Returns number of bytes stored (if > 0) or errno if <0
err_t ftp_put(u8_t *hostname, u8_t *username, u8_t *passwd, const u8_t *filename, u8_t *buf, u16_t len)
{
	struct netconn *ftpclientc = NULL;
	struct netconn *ftpclientd = NULL;
	u8_t controlbuf[FTPC_MSG_BUFF];		/* Buffer to exchange commands with server. */
	err_t stats;
	ip4_addr_t srvip;

	ftpclientc = netconn_new(NETCONN_TCP);	/* Open Netcomm to FTP Control Comm. */
	if(ftpclientc == NULL)
	{
		FTPC_ERROR("\nFTPC: Error in Open FTP Control Netconn!");
		return(FTP_BAD);
	}

	ip4addr_aton(hostname, &srvip);	/* Getting IP. */
	stats = netconn_connect(ftpclientc, &srvip, FTPSERVERPORTC);	/* Trying to connect to server. */
	if(stats == ERR_OK)
	{
		stats = ftpclientOpenSession(ftpclientc, username, passwd, controlbuf, sizeof(controlbuf));	/* Opening a FTP Session. */
		if (stats != ERR_OK)
		{
			return (stats);
		}

		ftpclientd = netconn_new(NETCONN_TCP);	/* Open Netcomm to FTP Data Comm. */
		if(ftpclientd == NULL)
		{
			/* ERROR! Close actual session. */
			stats = ftpclientCloseSession(ftpclientc, controlbuf, sizeof(controlbuf));
			FTPC_ERROR("\nFTPC: Error in Open FTP Data Netconn!");
			return(FTP_BAD);
		}

		/* We are now logged in and ready to transfer the file. Open the data socket
		ready to receive the file. It also builds the PASV command ready to send */
		stats = ftpclientOpenDataSession(ftpclientc, ftpclientd, controlbuf, sizeof(controlbuf));
		if (stats != ERR_OK)
		{
			FTPC_ERROR("\nFTPC: Error in Open FTP Data Section!");
			stats = ftpclientCloseSession(ftpclientc, controlbuf, sizeof(controlbuf));
			return (stats);
		}

		/* Check that file exist. */
		stats = command("SIZE", filename, ftpclientc, controlbuf, sizeof(controlbuf));

		/* Send file to store. */
		stats = command("STOR", filename, ftpclientc, controlbuf, sizeof(controlbuf));
		if ((stats < 0) || (stats != 1))
		{
			/* ERROR! Close actual session. */
			FTPC_ERROR("\nFTPC: STOR failed!");
			stats = ftpclientCloseSession(ftpclientc, controlbuf, sizeof(controlbuf));
			return (stats);
		}

		u16_t bytes;
		bytes = send_file(ftpclientd, buf, len);
		if (bytes < 0)
		{
			FTPC_DEBUG("\nFTPC: Sending file failed.");
			return (bytes);
		}

		FTPC_DEBUG("\n Number of bytes in sent file: %d\n",  bytes);

		stats = get_reply(ftpclientc, NULL, 0);
		if (stats != 2)	// expected: "226 Transfer OK\r\n"
		{
			FTPC_ERROR("\nFTPC: Transfer failed, code %d, answer:[%s]", stats, controlbuf);
			stats = ftpclientCloseSession(ftpclientc, controlbuf, sizeof(controlbuf));
			return (FTP_BAD);
		}

		FTPC_DEBUG("\nTransfer OK, file sent :) \n");

		stats = ftpclientCloseSession(ftpclientc, controlbuf, sizeof(controlbuf));
	}
	return (stats);
}

/*****************************************************************************/
TaskHandle_t ftpclientGetHandle(void)
{
	return (ftpclienthandle);
}

/*****************************************************************************/
void ftpclientSetHandle(TaskHandle_t handle)
{
	ftpclienthandle = handle;
}

/*****************************************************************************/
sys_thread_t ftpclientInit(void *ftpclientconfig, u16_t stacksize, u8_t taskprio)
{
	ftpclienthandle = sys_thread_new("ftpclient", ftpclientThread, ftpclientconfig, stacksize, taskprio);
	return(ftpclienthandle);
}

/******************************************************************************
 *                    IMPLEMENTACAO DAS FUNCOES PRIVADAS					  *
 *****************************************************************************/
static void ftpclientThread(void *arg)
{
	u8_t filebufget[20];

//	while(1)
	{
		ftp_get("192.168.2.50", "anonymous", "anonymous", "teste.txt", filebufget, sizeof(filebufget), NULL);
		ftp_put("192.168.2.50", "anonymous", "anonymous", "blah.txt", filebufget, sizeof(filebufget));
	}

	while(1);
}
/*****************************************************************************/
/* Receive the file into the buffer. */
static s8_t ftpclientReceivedData(struct netconn *dataconn, u8_t *buf, u16_t len, u16_t *bytes)
{
	s16_t ret;

	ret = receive_data(dataconn, buf, len, bytes);
	if((ret != ERR_OK) || (*bytes == 0) || (*bytes > len))
	{
		FTPC_DEBUG("\nFTPC: Erro na captura dos dados.");
		ret = FTP_BAD;
	}
	return (ret);
}

/////////////////////////////////////////////////////////////////////////////////
///* Send the file to the server. */
static s16_t send_file(struct netconn *dataconn, const u8_t *buf, u16_t bufsize)
{
	u16_t remaining = bufsize;
	err_t ret;
	u16_t sent = 0;

	while (1)
	{
		ret = send_data(dataconn, (void *)buf, bufsize);
		if (ret < 0)
		{
			FTPC_ERROR("\nFTPC: Send_file() error: %d\n", ret);
			netconn_close(dataconn);	/* signal EndOfFile. */
			return (ret);
		}

		sent += bufsize;
		if (sent == remaining)
			break;
	}

	netconn_close(dataconn);
	return (sent);
}

static s8_t get_line(struct netconn *conn, u8_t *buf, u16_t buf_size)
{
	struct pbuf *p;
	u16_t len = 0, cur_len;
	u8_t *crpos;
	err_t ret;
	u16_t lastelement;

	if ((buf == NULL) || (buf_size == 0))
		return (-1);

	lastelement = buf_size - 1;
	buf[lastelement] = '\0';

	ret = netconn_recv_tcp_pbuf(conn, &p);
	if (ret == ERR_OK)
	{
		pbuf_copy_partial(p, &buf[len], buf_size - len, 0);
		cur_len = p->tot_len;
		len += cur_len;
		pbuf_free(p);

		crpos = strchr(buf,'\r');
		if(crpos != NULL)
		{
			buf[crpos-buf] = '\0';
		}

		if ((buf[len-1] == '\r') && (len > 1))	// delete CR if any
			buf[len-1] = '\0';

		buf[len] = '\0';
	}
	return (ret); // OK
}

/* Read one line from the server, being careful not to overrun the
   buffer. If we do reach the end of the buffer, discard the rest of the line.
   Note: buf_size must be >=5 in order to hold the return code
*/
static int receive_data(struct netconn *conn, u8_t *buf, u16_t buf_size, u16_t *bytes)
{
	struct pbuf *p;
	err_t ret;

	*bytes = 0;
	ret = netconn_recv_tcp_pbuf(conn, &p);
	if (ret == ERR_OK)
	{
		*bytes = pbuf_copy_partial(p, &buf[0], buf_size, 0);
	}

	pbuf_free(p);

	return (ret);
}

/* Read the reply from the server and return the MSByte from the return
   code. This gives us a basic idea if the command failed/worked. The
   reply can be spread over multiple lines. When this happens the line
   will start with a - to indicate there is more

  http://www.wu-ftpd.org/rfc/rfc959.html

    The three digits of the reply each have a special significance.
	The first digit denotes whether the response is good, bad or incomplete.
    An unsophisticated user-process will be able to determine its next
	action by simply examining this first digit ....

	If (reply == NULL) OR (reply_size < 5), the reply will not be copied in *reply,
	only the MSByte	of the reply string will be returned.

	If you need the full return code, use strtoul(reply); note: strlen(reply) >= 5 bytes
*/
static int get_reply(struct netconn *conn, u8_t *reply, u16_t reply_size)
{
	u8_t *dest;
	u8_t local_buf[5];	// holds the beginning of the response when *reply is not supplied from upper-layer function
	u8_t more = 0;
	u8_t first_line = 1;
	u16_t len;
	int ret, code = 0;

	if ((reply != NULL) && (reply_size >= 5))
	{
		dest = reply;		// if there is higher-level buffer for messages available, then use it
		len = reply_size;
	}
	else
	{
   		dest = local_buf;	// else use the small local buffer in stack (only for result code)
		len = sizeof(local_buf);
	}

	{
		if ((ret = get_line(conn, dest, len)) != ERR_OK)
		{
			return(FTP_TIMEDOUT);		// should be FTP_BAD which is (-2) or FTP_TIMEDOUT (-8)
		}

		if (first_line)
		{
			code = strtoul(dest, NULL, 0);
			first_line = 0;
			more = (dest[3] == '-');
		}
		else
		{
			if(isdigit(dest[0]) &&
			   isdigit(dest[1]) &&
			   isdigit(dest[2]) && (code == (int)strtoul(dest, NULL, 0)) && dest[3] == ' ') // code of current line == code of first line
			{ more = 0; }
			else
			{ more = 1; }
		}

		FTPC_DEBUG("\nFTPServer Received: [%s]", dest);
	}
	return (dest[0] - '0');			// the MSByte of the return code
}

///////////////////////////////////////////////////////////////////////////////
// Send a command to the server
static int send_cmd(struct netconn *conn, const u8_t *str)
{
	err_t flag;

	flag = netconn_write(conn, (void *)str, strlen(str), NETCONN_COPY);

	if(flag != ERR_OK)
	{
		FTPC_ERROR("\nFTPC: Send_cmd() write error: %d.", flag);
		return (FTP_BAD);
	}
	return (ERR_OK);
}

// Send data to the file in the server
static int send_data(struct netconn *conn, const u8_t *buf, u16_t bufsize)
{
	err_t flag;
	struct netbuf *buffer;
	char * data;

	buffer = netbuf_new();
	data = netbuf_alloc(buffer, bufsize);
	memcpy (data, buf, bufsize);
	flag = netconn_write(conn, (void *)buffer, bufsize, NETCONN_NOCOPY);

	netbuf_delete(buffer);

	if(flag != ERR_OK)
	{
		FTPC_ERROR("\nFTPC: Send_cmd() write error: %d.", flag);
		return (FTP_BAD);
	}
	return (ERR_OK);
}

///////////////////////////////////////////////////////////////////////////////
/* Send a complete command to the server and receive the reply. Pass temp
   buffer (*msgbuf) for commands and answers. Return the MSB of the reply code. */
static s8_t command(const u8_t *cmd, const u8_t *arg, struct netconn *conn, u8_t *buf, u16_t len)
{
	int err, cmd_len;

	if (arg)
	{
		cmd_len = snprintf(buf, len, "%s %s\r\n", cmd, arg);
	}
	else
	{
		cmd_len = snprintf(buf, len, "%s\r\n", cmd);
	}
	//  cmd_len = number of characters that would have been written had 'msgbuflen'
	//	been sufficiently large, not counting the terminating null character

	if (cmd_len >= len)
	{
		FTPC_ERROR("\nFTPC: len size is smaller than %s", cmd);
		return (FTP_TOOBIG);
	}

	if (cmd_len <= 0)
	{
		FTPC_ERROR("\nFTPC: Conversion error for command %s", cmd);
		return (FTP_BAD);
	}

	if ( (err = send_cmd(conn, buf)) != ERR_OK )
	{
		FTPC_ERROR("\nFTPC: Could not send command %s", cmd);
		return(err);	// should be FTP_BAD or FTP_TOOBIG
	}

	FTPC_DEBUG("\nFTPC: Command sent: %s", buf);

	return (get_reply(conn, buf, len));
}

//////////////////////////////////////////////////////////////////////////////
/* Perform a login to the server. Pass the username and password and put the connection into binary mode.
   Control conection must be established in advance on sockfd. */
static s8_t login(u8_t *username, u8_t *passwd, struct netconn *conn, u8_t *buf, u16_t len)
{
	s8_t ret;

	ret = command("USER", username, conn, buf, len);	// returns the first digit of the response code , e.g 331 -> 3 ==User OK, need pass

	if ((buf[0] == '2') && (buf[1] == '3') && (buf[2] == '0')) // if answer is  "230 Logged on" (without password)
		goto _logged_in;			// hardly possible

	if (ret != 3)
	{
		FTPC_DEBUG("\nFTPC: User %s not accepted.", username);
		return (FTP_BADUSER);
	}

	ret = command("PASS", passwd, conn, buf, len);

	if (ret != 2)
	{
    	if (ret < 0)
			return (ret);	// socket or other error

		FTPC_DEBUG("\nFTP: Login failed for User %s.", username);
		return (FTP_BADUSER);
	}

_logged_in:
	FTPC_DEBUG("\nFTP: Login successful.");

	ret = command("TYPE", "I", conn, buf, len);	// set binary transfer mode

	if (ret != 2)
	{
    	if (ret < 0)
			return (ret);

		FTPC_ERROR("\nFTP: TYPE command failed!");
		return (FTP_BAD);
	}

	return (ret);	// 2
}
///////////////////////////////////////////////////////////////////////////////
// Returns the port on which the server waits for data connection or 0 if error occured
// by parsing the response of the PASV command,
// e.g "227 Entering Passive Mode (192,168,1,18,5,8)"
// If (ip_str != NULL) the IP is returned there as string ("192.168.1.18")
static u16_t GetDynPort(u8_t* msg, u8_t* ip_str, u32_t len)
{
	//	Note: strtok() will alter the IP/Port string by placing NULLS at
	//	the delimiter positions. A good habit is to copy the string
	//	to a temporary string and use the copy in the strtok() call.
	//	ip_str needs 17 bytes!

	u32_t port = 20;

	char local_buf[27];				// "(000,000,000,000,000,000)"
	char *pToken;
	u8_t i;
	u8_t ip1, ip2, ip3, ip4, p1, p2;
	char delimiter[2];

	FTPC_DEBUG("\nFTPC: Extracting server IP and port from msg [%s].", msg);

	i = 0;
	ip1 = ip2 = ip3 = ip4 = p1 = p2 = 0;

	while (i < len)
	{
		if ((msg[i] == '(') || (msg[i] == '[') || (msg[i] == '<') || (msg[i] == '{'))
		{
			memcpy(local_buf, (msg + i + 1), sizeof(local_buf));
			// local_buf gets string after the parenthesis (to digit or space)
			break;
		}
		i++;
		if (i >= len) {
			return (0);	// Could not parse string! Invalid server response
		}
	}
	// Question: Does anybody know if the delimiter is '.' or ',' ???
	// find delimiter (colon/point)
	delimiter[0] = ',';
	delimiter[1] = 0;

	if(!strchr(local_buf, delimiter[0]))
	{
		delimiter[0] = '.';		// try point
		if(!strchr(local_buf, delimiter[0]))
		{
			FTPC_ERROR("\nFTPC: Could not find delimiter!");
			return (0);	// return 0, FTP_BAD is interpreted as valid port number!
		}
	}

	i = 0;
	pToken = strtok(local_buf, delimiter);
	while (pToken)
	{
		switch(i++)
		{
			case 0: ip1 = atoi(pToken); break;
			case 1: ip2 = atoi(pToken); break;
			case 2: ip3 = atoi(pToken); break;
			case 3: ip4 = atoi(pToken); break;
			case 4: p1  = atoi(pToken); break;
			case 5: p2  = atoi(pToken); break;
			default:
				FTPC_DEBUG("\nFTPC: ERROR! COULD NOT PARSE PASV ANSWER! - [%s].", msg);
				return (0);
		}
		pToken = strtok(NULL, delimiter);
	}

	port = (p1*256 + p2);

	if (ip_str)									// report IP
		sprintf(ip_str, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

	FTPC_DEBUG("\nFTPC: FTP Server IP: %d.%d.%d.%d:%d", ip1, ip2, ip3, ip4, port);
 	return (port);
}
/******************************************************************************	
 *                                    EOF                                     *
 *****************************************************************************/
