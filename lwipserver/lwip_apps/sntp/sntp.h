#ifndef __SNTP_H__
#define __SNTP_H__

#include "lwip/inet.h"

/**
 * SNTP_DEBUG: Enable debugging for SNTP.
 */
#ifndef SNTP_DEBUG
#define SNTP_DEBUG                  LWIP_DBG_OFF
#endif

/** SNTP server port */
#ifndef SNTP_PORT
#define SNTP_PORT                   123
#endif

/** SNTP server address as IPv4 address in "u32_t" format */
#ifndef SNTP_SERVER_ADDRESS
#define SNTP_SERVER_ADDRESS         "167.160.167.167" /* your ntp server */
#endif

/** SNTP receive timeout - in milliseconds */
#ifndef SNTP_RECV_TIMEOUT
#define SNTP_RECV_TIMEOUT           3000
#endif

/** SNTP update delay - in milliseconds */
#ifndef SNTP_UPDATE_DELAY
#define SNTP_UPDATE_DELAY           10000	/* 5 minutos. */
#endif

/** SNTP macro to change system time and/or the update the RTC clock */
#ifndef SNTP_SYSTEM_TIME
#define SNTP_SYSTEM_TIME(t)
#endif

/* SNTP protocol defines */
#define SNTP_MAX_DATA_LEN           48
#define SNTP_RCV_TIME_OFS           32
#define SNTP_LI_NO_WARNING          0x00
#define SNTP_VERSION               (4/* NTP Version 4*/<<3)
#define SNTP_MODE_CLIENT            0x03
#define SNTP_MODE_SERVER            0x04
#define SNTP_MODE_BROADCAST         0x05
#define SNTP_MODE_MASK              0x07

/* number of seconds between 1900 and 1970 */
#define DIFF_SEC_1900_1970         (2208988800)

time_t sntpRequest(void);
sys_thread_t sntpInit(void *sntpconfig, u16_t stacksize, u8_t taskprio);
void rtcHardwareStart(void);

#endif /* __SNTP_H__ */
