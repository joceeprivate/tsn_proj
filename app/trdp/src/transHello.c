/**********************************************************************************************************************/
/**
 * @file            sendHello.c
 *
 * @brief           Demo application for TRDP
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Bernd Loehr and Florian Weispfenning, NewTec GmbH
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2013. All rights reserved.
 *
 * $Id: sendHello.c 1763 2018-09-21 16:03:13Z ahweiss $
 *
 *      BL 2018-03-06: Ticket #101 Optional callback function on PD send
 *      BL 2017-06-30: Compiler warnings, local prototypes added
 */

/***********************************************************************************************************************
 * INCLUDES
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "vos_thread.h"
#include "vos_utils.h"
#include "trdp_if_light.h"

#include "tadma_conf.h"
#include "thread_cmd.h"
#include "port_conf.h"


/************************************************************************************************************************
 * DEFINITIONS
 */
#define APP_VERSION "1.4"

#define DATA_MAX 1432u

#define PD_COMID 0u
#define PD_COMID_CYCLE 1000000u /* in us (1000000 = 1 sec) */

/* We use dynamic memory    */
#define RESERVED_MEMORY 1280000u

#define TRIGTIME_INTERVAL 50000u

CHAR8 gBuffer[DATA_MAX];

/***********************************************************************************************************************
 * PROTOTYPES
 */
int getopt(int argc, char * const argv[],
            const char *optstring);

extern char *optarg;

/**********************************************************************************************************************/
/** callback routine for TRDP logging/error output
 *
 *  @param[in]      pRefCon            user supplied context pointer
 *  @param[in]        category        Log category (Error, Warning, Info etc.)
 *  @param[in]        pTime            pointer to NULL-terminated string of time stamp
 *  @param[in]        pFile            pointer to NULL-terminated string of source module
 *  @param[in]        LineNumber        line
 *  @param[in]        pMsgStr         pointer to NULL-terminated string
 *  @retval         none
 */
void dbgOut(
    void *pRefCon,
    TRDP_LOG_T category,
    const CHAR8 *pTime,
    const CHAR8 *pFile,
    UINT16 LineNumber,
    const CHAR8 *pMsgStr)
{
    const char *catStr[] = {"**Error:", "Warning:", "   Info:", "  Debug:", "   User:"};
    printf("%s %s %s:%d %s",
           strrchr(pTime, '-') + 1,
           catStr[category],
           strrchr(pFile, VOS_DIR_SEP) + 1,
           LineNumber,
           pMsgStr);
}

/* Print a sensible usage message */
void usage(const char *appName)
{
    printf("Usage of %s\n", appName);
    printf("This tool sends PD messages to an ED.\n"
           "Arguments are:\n"
           "-o <own IP address> (default INADDR_ANY)\n"
           "-t <target IP address>\n"
           "-c <comId> (default 0)\n"
           "-s <cycle time> (default 1000000 [us])\n"
           "-n process number (default 1)\n"
           "-e send empty request\n"
           "-d data length, max 1430\n"
           "-v set various cycle (default false)\n");
}

/**********************************************************************************************************************/
/** main entry
 *
 *  @retval         0        no error
 *  @retval         1        some error
 */
int main(int argc, char *argv[])
{
    unsigned int ip[4];
    unsigned int processNum = 1;

    TRDP_APP_SESSION_T appHandle_send, appHandle_recv; /*    Our identifier to the library instance    */
    TRDP_PUB_T pubHandle;         /*    Our identifier to the publication         */
    PORT_HANDLE portHandle = NULL;


    unsigned int comId = PD_COMID;
    unsigned int cycleTime = PD_COMID_CYCLE;
    BOOL8 variousCycle = FALSE;
    TRDP_ERR_T err = 0;
    TRDP_PD_CONFIG_T pdConfiguration =
        {NULL, NULL, {0u, 64u, 0u}, TRDP_FLAGS_NONE, 1000000u, TRDP_TO_SET_TO_ZERO, 0};
    TRDP_MEM_CONFIG_T dynamicConfig = {NULL, RESERVED_MEMORY, {0}};
    TRDP_PROCESS_CONFIG_T processConfig = {"Me", "", 0, 0, TRDP_OPTION_BLOCK};
    UINT32 ownIP = vos_dottedIP("10.1.1.10");
    UINT32 sendIP = 0u;
    UINT32 recvIP = vos_dottedIP("10.1.1.20");

    /*    Generate some data, that we want to send, when nothing was specified. */    
    UINT8 exampleData[DATA_MAX] = "Hello World";
    UINT32 outputBufferSize = 24u;
    UINT8 *outputBuffer = exampleData;
    UINT32 dataSize;
    UINT8 data[DATA_MAX];

    INT32 sock;
    UINT32 triggerTime = 2500000u;
    int ch, i;

    /* threads */
    VOS_THREAD_T thread_pdRecv, thread_pdSend, thread_pdGet;

    for (i = 0; i < DATA_MAX; i++)
    {
        data[i] = (UINT8)i;
    }

    if (argc <= 1)
    {
        usage(argv[0]);
        return 1;
    }

    while ((ch = getopt(argc, argv, "o:t:c:n:d:s:h?ve")) != -1)
    {
        switch (ch)
        {
        case 'o':
        { /*  read ip    */
            if (sscanf(optarg, "%u.%u.%u.%u",
                       &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
            {
                usage(argv[0]);
                exit(1);
            }
            ownIP = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
            break;
        }
        case 'c':
        { /*  read comId    */
            if (sscanf(optarg, "%u",
                       &comId) < 1)
            {
                usage(argv[0]);
                exit(1);
            }
            break;
        }
        case 's':
        { /*  read cycle time    */
            if (sscanf(optarg, "%u",
                       &cycleTime) < 1)
            {
                usage(argv[0]);
                exit(1);
            }
            break;
        }
        case 'n':
        {
            /*  read process number   */
            if (sscanf(optarg, "%u",
                       &processNum) < 1)
            {
                usage(argv[0]);
                exit(1);
            }
            break;
        }
        case 't':
        { /*  read ip    */
            if (sscanf(optarg, "%u.%u.%u.%u",
                       &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
            {
                usage(argv[0]);
                exit(1);
            }
            sendIP = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
            if (sendIP == 0)
            {
                fprintf(stderr, "No destination address given!\n");
                usage(argv[0]);
                exit(1);
            }
            break;
        }
        case 'e':
        {
            outputBuffer = NULL;
            outputBufferSize = 0;
        }
        break;
        case 'd':
        { /*  data    */
            /*  send datasize    */
            if (sscanf(optarg, "%u",
                       &dataSize) < 1 ||
                dataSize > DATA_MAX)
            {
                usage(argv[0]);
                exit(1);
            }
            outputBuffer = data;
            outputBufferSize = dataSize;
            break;
        }
        case 'v':
            variousCycle = TRUE;
            break;
        case 'h':
        case '?':
        default:
            usage(argv[0]);
            return 1;
        }
    }

    printf(" SendTRDP : IP: %x, COMID: %d, NUM: %d ", sendIP, comId, processNum);    
    if (variousCycle)
        printf("VARIOUS CYCLE: 30/60/100/250 change every 32 COMID\n");
    else
        printf("CYCLE: %d\n", cycleTime);
    
    printf(" RecvTRDP : COMID: %d, NUM: 256\n", comId);

    /*    Init the library  */
    if (tlc_init(NULL,               /* (MODIFIED) &dbgOut for logging, set NULL for no logging */
                 NULL,
                 &dynamicConfig) != TRDP_NO_ERR) /* Use application supplied memory    */
    {
        printf("Initialization error\n");
        return 1;
    }

    /*    Open a session  */
    if (tlc_openSession(&appHandle_send,
                    ownIP, 0,               /* use default IP address           */
                    NULL,                   /* no Marshalling                   */
                    &pdConfiguration, NULL, /* system defaults for PD and MD    */
                    &processConfig) != TRDP_NO_ERR)
    {
        vos_printLogStr(VOS_LOG_USR, "Send Initialization error\n");
        return 1;
    }

    if (tlc_openSession(&appHandle_recv,
                    ownIP, 0,               /* use default IP address           */
                    NULL,                   /* no Marshalling                   */
                    &pdConfiguration, NULL, /* system defaults for PD and MD    */
                    &processConfig) != TRDP_NO_ERR)
    {
        vos_printLogStr(VOS_LOG_USR, "Recv Initialization error\n");
        return 1;
    }

    /*    Subscribe to control PD        */


    /*    Copy the packet into the internal send queue, prepare for sending.    */
    /*    If we change the data, just re-publish it    */

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        vos_printLogStr(VOS_LOG_USR, "SOCK Init error\n");
        goto out;
    }

    if (tadma_streamFlush(sock, "ep") != 0)
    {
        vos_printLogStr(VOS_LOG_USR, "TADMA flush stream error\n");
        goto out;
    }

    for (i = 0; i < processNum; i++)
    {
        if (variousCycle)
        {
            if (i < 32)
                cycleTime = 30000u;
            else if (i < 64)
                cycleTime = 60000u;
            else if (i < 96)
                cycleTime = 100000u;
            else
                cycleTime = 250000u;
        }

        /* TODO: Fix tadma driver for little-endian sendIP, now big-endian */
        if (tadma_streamAdd(sock,
                            "ep",
                            sendIP,
                            comId + i,
                            triggerTime,
                            1,
                            i ? FALSE : TRUE) != 0)                                
        {
            vos_printLog(VOS_LOG_USR, "TADMA add stream error, errno = %d\n", errno);
            goto out;
        }

        triggerTime += TRIGTIME_INTERVAL; 

        err = tlp_publish(appHandle_send,  /*    our application identifier    */
                          &pubHandle, /*    our pulication identifier     */
                          comId + i,             /*    ComID to send                 */
                          0,                     /*    etbTopoCnt = 0 for local consist only     */
                          0,                     /*    opTopoCnt = 0 for non-directinal data     */
                          ownIP,                 /*    default source IP             */
                          sendIP,                /*    where to send to              */
                          cycleTime,             /*    Cycle time in us              */
                          0,                     /*    not redundant                 */
                          TRDP_FLAGS_NONE,       /*    Use callback for errors       */
                          NULL,                  /*    default qos and ttl           */
                          (UINT8 *)outputBuffer, /*    initial data                  */
                          outputBufferSize       /*    data size                     */
        );

        if (err != TRDP_NO_ERR)
        {
            vos_printLog(VOS_LOG_ERROR, "prep pub pd error, err code = %d\n", err);
            goto out;
        }
    }

    if (tadma_streamProgram(sock, "ep") != 0)
    {
        vos_printLogStr(VOS_LOG_USR, "TADMA program stream error\n");
        goto out;
    }

    close(sock);

    for (i = 0; i < 256; i ++)
    {
        err = port_subscribe(&portHandle,
                            i,
                            appHandle_recv,
                            comId + i,
                            recvIP,
                            ownIP,
                            30000,
                            150000);

        if (err != TRDP_NO_ERR)
        {
            vos_printLog(VOS_LOG_ERROR, "port sub error, err code = %d\n", err);
            goto out;
        }
    }

    /* policy : default, interval : 0, no other options */
    err = vos_threadCreate(&thread_pdSend, "pd_send", VOS_THREAD_POLICY_OTHER, 0, 0, 0, &thread_process_pdSend, appHandle_send);
    if (err != VOS_NO_ERR)
    {
        printf("SEND thread created failed!!\n");
        goto out;
    }

    err = vos_threadCreate(&thread_pdRecv, "pd_recv", VOS_THREAD_POLICY_OTHER, 0, 0, 0, &thread_process_pdRecv, appHandle_recv);
    if (err != VOS_NO_ERR)
    {
        printf("RECV thread created failed!!\n");
        goto out;
    }

    err = vos_threadCreate(&thread_pdGet, "pd_get", VOS_THREAD_POLICY_OTHER, 0, 0, 0, &thread_process_pdGet, portHandle);
    if (err != VOS_NO_ERR)
    {
        printf("GET thread created failed!!\n");
        goto out;
    }

    printf("Thread Init Done!\n");

    /*
       Enter the main processing loop.
     */

    while(1)
    {
        sleep(3);
    }

    /*
     *    We always clean up behind us!
     */
out:
    close(sock);
    tlc_closeSession(appHandle_send);
    tlc_closeSession(appHandle_recv);
    tlc_terminate();
    return err;
}
