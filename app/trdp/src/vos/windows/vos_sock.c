/**********************************************************************************************************************/
/**
 * @file            windows/vos_sock.c
 *
 * @brief           Socket functions
 *
 * @details         OS abstraction of IP socket functions for UDP and TCP
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Bernd Loehr, NewTec GmbH
 *
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2013. All rights reserved.
 *
 * $Id: vos_sock.c 1833 2019-01-29 13:18:48Z bloehr $*
 *
 *      BL 2017-05-22: Ticket #122: Addendum for 64Bit compatibility (VOS_TIME_T -> VOS_TIMEVAL_T)
 */

#ifndef WIN32
#error \
    "You are trying to compile the WIN32 implementation of vos_sock.c - either define WIN32 or exclude this file!"
#endif

/***********************************************************************************************************************
 * INCLUDES
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <ws2def.h>
#include <MSWSock.h>
#include <lm.h>
#include <iphlpapi.h>

#include "vos_utils.h"
#include "vos_sock.h"
#include "vos_thread.h"
#include "vos_mem.h"

#pragma comment(lib, "IPHLPAPI.lib")

/* include the needed windows network libraries */
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

/***********************************************************************************************************************
 * DEFINITIONS
 */
#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

/* Defines for recvmsg: */

#define msghdr          _WSAMSG
#define msg_name        name
#define msg_namelen     namelen
#define msg_iov         lpBuffers
#define msg_iovlen      dwBufferCount
#define msg_flags       dwFlags
#define msg_control     Control.buf
#define msg_controllen  Control.len

#define iovec           _WSABUF
#define iov_base        buf
#define iov_len         len

#define CMSG_FIRSTHDR   WSA_CMSG_FIRSTHDR
#define CMSGSize        64       /* size of buffer for destination address */

/***********************************************************************************************************************
 *  LOCALS
 */

BOOL8   vosSockInitialised = FALSE;
UINT8   mac[VOS_MAC_SIZE];


/***********************************************************************************************************************
 * LOCAL FUNCTIONS
 */

/**********************************************************************************************************************/
/** Receive a message including sender address information.
 *
 *  @param[in]      sock            socket descriptor
 *  @param[in]      pMessage        Pointer to message header
 *  @param[in]      flags           Receive flags
 *
 *  @retval         number of received bytes, -1 for error
 */
INT32 recvmsg (int sock, struct msghdr *pMessage, int flags)
{
    GUID    WSARecvMsg_GUID = WSAID_WSARECVMSG;
    LPFN_WSARECVMSG WSARecvMsg;
    DWORD   numBytes    = 0;
    int     res         = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                                   &WSARecvMsg_GUID, sizeof(WSARecvMsg_GUID), &WSARecvMsg,
                                   sizeof(WSARecvMsg), &numBytes, NULL, NULL);

    pMessage->dwFlags = flags;
    res = WSARecvMsg(sock, pMessage, &numBytes, NULL, NULL);
    if (0 != res)
    {
        DWORD err = WSAGetLastError();
        if (err != WSAEMSGSIZE)
        {
            return -1;
        }
    }
    return numBytes;
}

/**********************************************************************************************************************/
/** Enlarge send and receive buffers to TRDP_SOCKBUF_SIZE if necessary.
 *
 *  @param[in]      sock            socket descriptor
 *
 *  @retval         VOS_NO_ERR       no error
 *  @retval         VOS_SOCK_ERR     buffer size can't be set
 */
VOS_ERR_T vos_sockSetBuffer (INT32 sock)
{
    int optval      = 0;
    int option_len  = sizeof(optval);

    (void)getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&optval, &option_len);
    if (optval < TRDP_SOCKBUF_SIZE)
    {
        optval = TRDP_SOCKBUF_SIZE;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char *)&optval, option_len) == -1)
        {
            (void)getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&optval, &option_len);
            vos_printLog(VOS_LOG_WARNING, "Send buffer size out of limit (max: %u)\n", optval);
            return VOS_SOCK_ERR;
        }
    }
    vos_printLog(VOS_LOG_INFO, "Send buffer limit = %u\n", optval);

    (void)getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&optval, &option_len);
    if (optval < TRDP_SOCKBUF_SIZE)
    {
        optval = TRDP_SOCKBUF_SIZE;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char *)&optval, option_len) == -1)
        {
            (void)getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&optval, &option_len);
            vos_printLog(VOS_LOG_WARNING, "Recv buffer size out of limit (max: %u)\n", optval);
            return VOS_SOCK_ERR;
        }
    }
    vos_printLog(VOS_LOG_INFO, "Recv buffer limit = %u\n", optval);

    return VOS_NO_ERR;
}

/***********************************************************************************************************************
 * GLOBAL FUNCTIONS
 */

/**********************************************************************************************************************/
/** Byte swapping.
 *
 *  @param[in]          val   Initial value.
 *
 *  @retval             swapped value
 */

EXT_DECL UINT16 vos_htons (
    UINT16 val)
{
    return htons(val);
}

EXT_DECL UINT16 vos_ntohs (
    UINT16 val)
{
    return ntohs(val);
}

EXT_DECL UINT32 vos_htonl (
    UINT32 val)
{
    return htonl(val);
}

EXT_DECL UINT32 vos_ntohl (
    UINT32 val)
{
    return ntohl(val);
}

/**********************************************************************************************************************/
/** Convert IP address from dotted dec. to !host! endianess
 *
 *  @param[in]          pDottedIP     IP address as dotted decimal.
 *
 *  @retval             address in UINT32 in host endianess
 */
EXT_DECL UINT32 vos_dottedIP (
    const CHAR8 *pDottedIP)
{
    UINT32 conversionResult;
    conversionResult = inet_addr(pDottedIP);
    /* Accd. MSDN the values INADDR_NONE and INADDR_ANY */
    /* indicate conversion errors, therefore zero has   */
    /* to be returned in order to have similar behavior */
    /* as by using inet_aton                            */
<<<<<<< .working
    if (    (conversionResult == INADDR_ANY)
         || (conversionResult == INADDR_NONE))
||||||| .merge-left.r1841
    if ((conversionResult == INADDR_ANY)
        || (conversionResult == INADDR_NONE))
=======
    if ((conversionResult == VOS_INADDR_ANY)
        ||
        (conversionResult == VOS_INADDR_ANY))
>>>>>>> .merge-right.r1661
    {
        return 0;
    }
    else
    {
        return vos_ntohl(inet_addr(pDottedIP));
    }
}

/**********************************************************************************************************************/
/** Convert IP address to dotted dec. from !host! endianess.
 *
 *  @param[in]          ipAddress   address in UINT32 in host endianess
 *
 *  @retval             IP address as dotted decimal.
 */

EXT_DECL const CHAR8 *vos_ipDotted (
    UINT32 ipAddress)
{
    static CHAR8 dotted[16];

    (void) vos_snprintf(dotted, sizeof(dotted), "%u.%u.%u.%u", (ipAddress >> 24), ((ipAddress >> 16) & 0xFF),
                        ((ipAddress >> 8) & 0xFF), (ipAddress & 0xFF));
    return dotted;
}

/**********************************************************************************************************************/
/** Check if the supplied address is a multicast group address.
 *
 *  @param[in]          ipAddress   IP address to check.
 *
 *  @retval             TRUE        address is multicast
 *  @retval             FALSE       address is not a multicast address
 */

EXT_DECL BOOL8 vos_isMulticast (
    UINT32 ipAddress)
{
    return IN_MULTICAST(ipAddress);
}

/**********************************************************************************************************************/
/** Get a list of interface addresses
 *  The caller has to provide an array of interface records to be filled.
 *
 *  @param[in,out]  pAddrCnt          in:   pointer to array size of interface record
 *                                    out:  pointer to number of interface records read
 *  @param[in,out]  ifAddrs           array of interface records
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   pAddrCnt and/or ifAddrs == NULL
 *  @retval         VOS_MEM_ERR     memory allocation error
 *  @retval         VOS_SOCK_ERR    GetAdaptersInfo() error
 */
EXT_DECL VOS_ERR_T vos_getInterfaces (
    UINT32          *pAddrCnt,
    VOS_IF_REC_T    ifAddrs[])
{
    UINT8               *buf            = NULL;
    UINT32              bufLen          = (UINT32) NULL;
    PIP_ADAPTER_INFO    pAdapterList    = NULL;
    PIP_ADAPTER_INFO    pAdapter        = NULL;
    UINT32              err         = 0;
    UINT32              addrCnt     = 0;
    DWORD               dwSize      = 0;
    DWORD               dwRetVal    = 0;
    MIB_IFTABLE         *pIfTable;
    VOS_ERR_T           retVal = VOS_NO_ERR;

    if ((pAddrCnt == NULL)
        || (ifAddrs == NULL))
    {
        return VOS_PARAM_ERR;
    }
    /* determine required buffer size, therefore no error check */
    err = GetAdaptersInfo(pAdapterList, (PULONG)&bufLen);
    buf = vos_memAlloc(bufLen);
    if (buf == NULL)
    {
        return VOS_MEM_ERR;
    }
    pAdapterList = (PIP_ADAPTER_INFO) buf;

    /* get the actual data we want */
    err = GetAdaptersInfo(pAdapterList, (PULONG)&bufLen);
    if (err != NO_ERROR)
    {
        vos_printLog(VOS_LOG_ERROR, "GetAdaptersInfo failed (Err: %d)\n", err);
        vos_memFree(buf);
        return VOS_SOCK_ERR;
    }
    pAdapter = pAdapterList;

    /* Iterate adapter list */
    while (pAdapter != NULL)
    {
        /* Only consider ethernet adapters (no loopback adapters etc.) */
        if (pAdapter->Type == MIB_IF_TYPE_ETHERNET)
        {
            /* store interface information if address will fit into output array */
            if (addrCnt < *pAddrCnt)
            {
                /* Store IP address */
                ifAddrs[addrCnt].ipAddr = vos_dottedIP(pAdapter->IpAddressList.IpAddress.String);
                /* Store MAC address */
                if (pAdapter->AddressLength != sizeof(ifAddrs[addrCnt].mac))
                {
                    memset(ifAddrs[addrCnt].mac, 0, sizeof(ifAddrs[addrCnt].mac));
                }
                else
                {
                    memcpy(ifAddrs[addrCnt].mac, pAdapter->Address, sizeof(ifAddrs[addrCnt].mac));
                }
                /* Store adapter name */
                (void) strncpy_s(ifAddrs[addrCnt].name, sizeof(ifAddrs[addrCnt].name), pAdapter->AdapterName, _TRUNCATE);
                /* Store subnet mask */
                ifAddrs[addrCnt].netMask = vos_dottedIP(pAdapter->IpAddressList.IpMask.String);
                /* increment number of addresses stored */
                addrCnt++;
            }
        }
        pAdapter = pAdapter->Next;
    }
    *pAddrCnt = addrCnt;
    vos_memFree(buf);

    /* get link/up down information from interface table */
    pIfTable = (MIB_IFTABLE *) vos_memAlloc(sizeof (MIB_IFTABLE));
    if (pIfTable == NULL)
    {
        vos_printLog(VOS_LOG_ERROR, "Error allocating memory\n");
        return (VOS_MEM_ERR);
    }

    /* Make an initial call to GetIfTable to get the necessary size into dwSize */
    dwSize = sizeof (MIB_IFTABLE);
    if (GetIfTable(pIfTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
    {
        vos_memFree(pIfTable);
        pIfTable = (MIB_IFTABLE *) vos_memAlloc(dwSize);
        if (pIfTable == NULL)
        {
            vos_printLog(VOS_LOG_ERROR, "Error allocating memory\n");
            return (VOS_MEM_ERR);
        }
    }

    /* Make a second call to GetIfTable to get the actual data we want.*/
    if ((dwRetVal = GetIfTable(pIfTable, &dwSize, 0)) == NO_ERROR)
    {
        if (pIfTable->dwNumEntries > 0)
        {
            MIB_IFROW       *pIfRow;
            unsigned int    i, j;


            pIfRow = (MIB_IFROW *) vos_memAlloc(sizeof (MIB_IFROW));

            if (pIfRow == NULL)
            {
                vos_printLog(VOS_LOG_ERROR, "Error allocating memory\n");
                if (pIfTable != NULL)
                {
                    vos_memFree(pIfTable);
                    pIfTable = NULL;
                }
                return (VOS_MEM_ERR);
            }

            for (i = 0; i < pIfTable->dwNumEntries; i++)
            {
                pIfRow->dwIndex = pIfTable->table[i].dwIndex;
                if ((dwRetVal = GetIfEntry(pIfRow)) == NO_ERROR)
                {
                    UINT8   physAddr[VOS_MAC_SIZE];
                    BOOL8   mismatch;

                    for (j = 0; (j < pIfRow->dwPhysAddrLen) && (j < VOS_MAC_SIZE); j++)
                    {
                        physAddr[j] = (UINT8) pIfRow->bPhysAddr[j];
                    }

                    for (addrCnt = 0; addrCnt < *pAddrCnt; addrCnt++)
                    {
                        mismatch = FALSE;

                        for (j = 0; j < VOS_MAC_SIZE; j++)
                        {
                            if (ifAddrs[addrCnt].mac[j] != physAddr[j])
                            {
                                mismatch = TRUE;
                            }
                        }

                        if (mismatch == FALSE)
                        {
                            for (j = 0; j < VOS_MAX_IF_NAME_SIZE - 1; j++)
                            {
                                ifAddrs[addrCnt].name[j] = pIfRow->bDescr[j];
                            }
                            ifAddrs[addrCnt].name[VOS_MAX_IF_NAME_SIZE - 1] = '\0';
                            if (pIfRow->dwOperStatus == IF_OPER_STATUS_OPERATIONAL)
                            {
                                ifAddrs[addrCnt].linkState = TRUE;
                            }
                            else
                            {
                                ifAddrs[addrCnt].linkState = FALSE;
                            }
                        }
                    }
                }
            }

            vos_memFree(pIfRow);
        }
        else
        {
            vos_printLog(VOS_LOG_ERROR, "\tGetIfTable failed with error: %ld\n", dwRetVal);
        }
    }

    vos_memFree(pIfTable);

    return retVal;
}

/**********************************************************************************************************************/
/** Get the state of an interface
 *
 *
 *  @param[in]      ifAddress       address of interface to check
 *
 *  @retval         TRUE            interface is up and ready
 *                  FALSE           interface is down / not ready
 */
EXT_DECL BOOL8 vos_netIfUp (
    VOS_IP4_ADDR_T ifAddress)
{
    /* tbd */
    return TRUE;
}

/**********************************************************************************************************************/
/** select function.
 *  Set the ready sockets in the supplied sets.
 *    Note: Some target systems might define this function as NOP.
 *
 *  @param[in]      highDesc          max. socket descriptor + 1
 *  @param[in,out]  pReadableFD       pointer to readable socket set
 *  @param[in,out]  pWriteableFD      pointer to writeable socket set
 *  @param[in,out]  pErrorFD          pointer to error socket set
 *  @param[in]      pTimeOut          pointer to time out value
 *
 *  @retval         number of ready file descriptors
 */

EXT_DECL INT32 vos_select (
    INT32           highDesc,
    VOS_FDS_T       *pReadableFD,
    VOS_FDS_T       *pWriteableFD,
    VOS_FDS_T       *pErrorFD,
    VOS_TIMEVAL_T   *pTimeOut)
{
    return select(highDesc, (fd_set *) pReadableFD, (fd_set *) pWriteableFD,
                  (fd_set *) pErrorFD, (struct timeval *) pTimeOut);
}

/*    Sockets    */

/**********************************************************************************************************************/
/** Initialize the socket library.
 *  Must be called once before any other call
 *
 *  @retval         VOS_NO_ERR            no error
 *  @retval         VOS_SOCK_ERR        sockets not supported
 */

EXT_DECL VOS_ERR_T vos_sockInit (void)
{
    WSADATA WsaDat;

    /* The windows socket library has to be prepared, before it could be used */
    if (WSAStartup(MAKEWORD(2, 2), &WsaDat) != 0)
    {
        int err = WSAGetLastError();

        err = err; /* for lint */
        vos_printLog(VOS_LOG_ERROR, "WSAStartup() failed (Err: %d)\n", err);
        return VOS_SOCK_ERR;
    }

    memset(mac, 0, sizeof(mac));
    vosSockInitialised = TRUE;

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** De-Initialize the socket library.
 *  Must be called after last socket call
 *
 */

EXT_DECL void vos_sockTerm (void)
{
    vosSockInitialised = FALSE;
}


/**********************************************************************************************************************/
/** Return the MAC address of the default adapter.
 *
 *  @param[out]     pMAC            return MAC address.
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   pMAC == NULL
 *  @retval         VOS_SOCK_ERR    socket not available or option not supported
 */

EXT_DECL VOS_ERR_T vos_sockGetMAC (
    UINT8 pMAC[VOS_MAC_SIZE])
{
    UINT32 i;

    if (!vosSockInitialised)
    {
        return VOS_INIT_ERR;
    }

    if (pMAC == NULL)
    {
        vos_printLog(VOS_LOG_ERROR, "Parameter error");
        return VOS_PARAM_ERR;
    }

    /*    Has it been determined before?    */
    for (i = 0; (i < sizeof(mac)) && (mac[i] != 0); i++)
    {
        ;
    }

    if (i < 6) /*    needs to be determined    */
    {
        /* for NetBIOS */
        DWORD   dwEntriesRead;
        DWORD   dwTotalEntries;
        BYTE    *pbBuffer;
        WKSTA_TRANSPORT_INFO_0  *pwkti;

        /* Get MAC address via NetBIOS's enumerate function */
        NET_API_STATUS          dwStatus = NetWkstaTransportEnum(
                NULL,                     /* [in] server name */
                0,                        /* [in] data structure to return */
                &pbBuffer,                /* [out] pointer to buffer */
                MAX_PREFERRED_LENGTH,     /* [in] maximum length */
                &dwEntriesRead,           /* [out] counter of elements actually enumerated */
                &dwTotalEntries,          /* [out] total number of elements that could be enumerated */
                NULL);                    /* [in/out] resume handle */

        if (dwStatus != 0)
        {
            memset(pMAC, 0, sizeof(mac));
            return VOS_UNKNOWN_ERR;
        }
        else
        {
            pwkti   = (WKSTA_TRANSPORT_INFO_0 *)pbBuffer;
            pwkti   = pwkti;  /* to make lint happy */

            /* skip all MAC addresses like 00000000 and search the first suitable one */
            for (i = 0; i < dwEntriesRead; i++)
            {
                (void) swscanf_s(
                    (wchar_t *)pwkti[i].wkti0_transport_address,
                    L"%2hx%2hx%2hx%2hx%2hx%2hx",
                    &mac[0],
                    &mac[1],
                    &mac[2],
                    &mac[3],
                    &mac[4],
                    &mac[5]);

                if (mac[0] != 0
                    || mac[1] != 0
                    || mac[2] != 0
                    || mac[3] != 0
                    || mac[4] != 0
                    || mac[5] != 0)
                {
                    /* at least one byte is set, stop the search */
                    break;
                }
            }
        }

        /* Release pbBuffer allocated by NetWkstaTransportEnum */
        dwStatus = NetApiBufferFree(pbBuffer); /*lint !e550 return value not used*/
    }

    memcpy(pMAC, mac, sizeof(mac));
    return VOS_NO_ERR;
}


/**********************************************************************************************************************/
/** Create an UDP socket.
 *  Return a socket descriptor for further calls. The socket options are optional and can be
 *    applied later.
 *    Note: Some targeted systems might not support every option.
 *
 *  @param[out]     pSock           pointer to socket descriptor returned
 *  @param[in]      pOptions        pointer to socket options (optional)
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   pSock == NULL
 *  @retval         VOS_SOCK_ERR    socket not available or option not supported
 */

EXT_DECL VOS_ERR_T vos_sockOpenUDP (
    INT32                   *pSock,
    const VOS_SOCK_OPT_T    *pOptions)
{
    SOCKET sock;

    if (!vosSockInitialised)
    {
        return VOS_INIT_ERR;
    }

    if (pSock == NULL)
    {
        vos_printLog(VOS_LOG_ERROR, "Parameter error");
        return VOS_PARAM_ERR;
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET )
    {
        int err = WSAGetLastError();

        err = err; /* for lint */
        vos_printLog(VOS_LOG_ERROR, "socket() failed (Err: %d)\n", err);
        return VOS_SOCK_ERR;
    }

    if ((vos_sockSetOptions(sock, pOptions))
        || (vos_sockSetBuffer(sock) != VOS_NO_ERR))
    {
        (void) closesocket(sock);
        return VOS_SOCK_ERR;
    }

    *pSock = (INT32) sock;
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Create a TCP socket.
 *  Return a socket descriptor for further calls. The socket options are optional and can be
 *    applied later.
 *
 *  @param[out]     pSock          pointer to socket descriptor returned
 *  @param[in]      pOptions       pointer to socket options (optional)
 *
 *  @retval         VOS_NO_ERR     no error
 *  @retval         VOS_PARAM_ERR  pSock == NULL
 *  @retval         VOS_SOCK_ERR   socket not available or option not supported
 */

EXT_DECL VOS_ERR_T vos_sockOpenTCP (
    INT32                   *pSock,
    const VOS_SOCK_OPT_T    *pOptions)
{
    SOCKET sock;

    if (!vosSockInitialised)
    {
        return VOS_INIT_ERR;
    }

    if (pSock == NULL)
    {
        vos_printLog(VOS_LOG_ERROR, "Parameter error");
        return VOS_PARAM_ERR;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET )
    {
        int err = WSAGetLastError();

        err = err; /* for lint */
        vos_printLog(VOS_LOG_ERROR, "socket() failed (Err: %d)\n", err);
        return VOS_SOCK_ERR;
    }

    if ((vos_sockSetOptions(sock, pOptions) != VOS_NO_ERR)
        || (vos_sockSetBuffer(sock) != VOS_NO_ERR))
    {
        (void) closesocket(sock);
        return VOS_SOCK_ERR;
    }

    *pSock = (INT32) sock;
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Close a socket.
 *  Release any resources aquired by this socket
 *
 *  @param[in]      sock            socket descriptor
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown
 */

EXT_DECL VOS_ERR_T vos_sockClose (
    INT32 sock)
{
    if (closesocket(sock) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();

        err = err; /* for lint */
        vos_printLog(VOS_LOG_ERROR, "closesocket() failed (Err: %d)\n");
        return VOS_PARAM_ERR;
    }
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Set socket options.
 *  Note: Some targeted systems might not support every option.
 *
 *  @param[in]      sock            socket descriptor
 *  @param[in]      pOptions        pointer to socket options (optional)
 *
 *  @retval         VOS_NO_ERR       no error
 *  @retval         VOS_PARAM_ERR    sock descriptor unknown
 */

EXT_DECL VOS_ERR_T vos_sockSetOptions (
    INT32                   sock,
    const VOS_SOCK_OPT_T    *pOptions)
{
    if (pOptions)
    {
        if (1 == pOptions->reuseAddrPort)
        {
            BOOL8 optValue = TRUE;
#ifdef SO_REUSEPORT
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &sockOptValue,
                           sizeof(sockOptValue)) == SOCKET_ERROR )
            {
                int err = WSAGetLastError();

                vos_printLog(VOS_LOG_ERROR, "setsockopt() SO_REUSEPORT failed (Err: %d)\n", err);
            }
#else
            if (setsockopt((SOCKET)sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&optValue,
                           sizeof(optValue)) == SOCKET_ERROR )
            {
                int err = WSAGetLastError();

                err = err; /* for lint */
                vos_printLog(VOS_LOG_ERROR, "setsockopt() SO_REUSEADDR failed (Err: %d)\n", err);
            }
#endif
        }

        if (pOptions->nonBlocking == TRUE)
        {
            u_long optValue = TRUE;

            if (ioctlsocket(sock, (long) FIONBIO, &optValue) == SOCKET_ERROR)
            {
                int err = WSAGetLastError();

                err = err; /* for lint */
                vos_printLog(VOS_LOG_ERROR, "setsockopt() FIONBIO failed (Err: %d)\n", err);
                return VOS_SOCK_ERR;
            }
        }

        if (pOptions->qos > 0 && pOptions->qos < 8)
        {
            /* The QoS value (0-7) is mapped to MSB bits 7-5, bit 2 is set for local use */
            DWORD optValue = (int) ((pOptions->qos << 5) | 4);
            if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_TOS, (const char *)&optValue,
                           sizeof(optValue)) == SOCKET_ERROR )
            {
                int err = WSAGetLastError();

                err = err; /* for lint */
                vos_printLog(VOS_LOG_ERROR, "setsockopt() IP_TOS failed (Err: %d)\n", err);
            }
        }
        if (pOptions->ttl > 0)
        {
            DWORD optValue = pOptions->ttl;
            if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_TTL, (const char *)&optValue,
                           sizeof(optValue)) == SOCKET_ERROR)
            {
                int err = WSAGetLastError();

                err = err; /* for lint */
                vos_printLog(VOS_LOG_ERROR, "setsockopt() IP_TTL failed (Err: %d)\n", err);
            }
        }
        if (pOptions->ttl_multicast > 0)
        {
            DWORD optValue = pOptions->ttl_multicast;
            if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&optValue,
                           sizeof(optValue)) == SOCKET_ERROR )
            {
                int err = WSAGetLastError();

                err = err; /* for lint */
                vos_printLog(VOS_LOG_ERROR, "setsockopt() IP_MULTICAST_TTL failed (Err: %d)\n", err);
            }
        }
        if (pOptions->no_mc_loop > 0)
        {
            /* Default behavior is ON * / */
            DWORD optValue = 0;
            if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&optValue,
                           sizeof(optValue)) == SOCKET_ERROR)
            {
                int err = WSAGetLastError();

                err = err; /* for lint */
                vos_printLog(VOS_LOG_ERROR, "setsockopt() IP_MULTICAST_LOOP failed (Err: %d)\n", err);
            }
        }
        if (pOptions->no_udp_crc > 0)
        {
            DWORD optValue = 0;
            if (setsockopt(sock, IPPROTO_UDP, UDP_CHECKSUM_COVERAGE, (const char *)&optValue,
                           sizeof(optValue)) == -1)
            {
                int err = WSAGetLastError();

                err = err; /* for lint */
                vos_printLog(VOS_LOG_ERROR, "setsockopt() UDP_CHECKSUM_COVERAGE failed (Err: %d)\n", err);
            }
        }

    }
    /*   This seems to be unsupported on XP (but IP_RECVDSTADDR is defined!)   */
    /*  Include struct in_pktinfo in the message "ancilliary" control data.
        This way we can get the destination IP address for received UDP packets */
    {
        DWORD optValue = TRUE;
        if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_PKTINFO, (const char *)&optValue, sizeof(optValue)) == -1)
        {
            int err = WSAGetLastError();
            err = err;     /* for lint */
            vos_printLog(VOS_LOG_ERROR, "setsockopt() IP_RECVDSTADDR failed (Err: %d)\n", err);
        }
    }

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Join a multicast group.
 *  Note: Some targeted systems might not support this option.
 *
 *  @param[in]      sock            socket descriptor
 *  @param[in]      mcAddress       multicast group to join
 *  @param[in]      ipAddress       depicts interface on which to join, default 0 for any
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown, parameter error
 *  @retval         VOS_SOCK_ERR    option not supported
 */

EXT_DECL VOS_ERR_T vos_sockJoinMC (
    INT32   sock,
    UINT32  mcAddress,
    UINT32  ipAddress)
{
    struct ip_mreq  mreq;
    VOS_ERR_T       result = VOS_NO_ERR;

    if (sock == (INT32)INVALID_SOCKET)
    {
        result = VOS_PARAM_ERR;
    }
    /*    Is this a multicast address?    */
    else if (IN_MULTICAST(mcAddress))
    {
        mreq.imr_multiaddr.s_addr   = vos_htonl(mcAddress);
        mreq.imr_interface.s_addr   = vos_htonl(ipAddress);

        {
            char    mcStr[16];
            char    ifStr[16];

            (void) strcpy_s(mcStr, sizeof(mcStr), inet_ntoa(mreq.imr_multiaddr));
            (void) strcpy_s(ifStr, sizeof(mcStr), inet_ntoa(mreq.imr_interface));
            vos_printLog(VOS_LOG_INFO, "joining MC: %s on iface %s\n", mcStr, ifStr);
        }

        if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) == SOCKET_ERROR)
        {
            int err = WSAGetLastError();

            if (WSAEADDRINUSE != err)
            {
                vos_printLog(VOS_LOG_ERROR, "setsockopt() IP_ADD_MEMBERSHIP failed (Err: %d)\n", err);
                result = VOS_SOCK_ERR;
            }
        }
        else
        {
            result = VOS_NO_ERR;
        }

        /* Disable multicast loop back */
        /* only useful for streaming
        {
            mreq.imr_multiaddr.s_addr   = 0;
            mreq.imr_interface.s_addr   = 0;

            if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&mreq, sizeof(mreq)) == SOCKET_ERROR)
            {
                int err = WSAGetLastError();

                vos_printLog(VOS_LOG_ERROR, "setsockopt() IP_MULTICAST_LOOP failed (Err: %d)\n", result);
                result = VOS_SOCK_ERR;
            }
            else
            {
                result = (result == VOS_SOCK_ERR) ? VOS_SOCK_ERR : VOS_NO_ERR;
            }
        }
        */
    }
    else
    {
        result = VOS_PARAM_ERR;
    }

    return result;
}

/**********************************************************************************************************************/
/** Leave a multicast group.
 *  Note: Some targeted systems might not support this option.
 *
 *  @param[in]      sock            socket descriptor
 *  @param[in]      mcAddress       multicast group to join
 *  @param[in]      ipAddress       depicts interface on which to leave, default 0 for any
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown, parameter error
 *  @retval         VOS_SOCK_ERR    option not supported
 */

EXT_DECL VOS_ERR_T vos_sockLeaveMC (
    INT32   sock,
    UINT32  mcAddress,
    UINT32  ipAddress)
{
    struct ip_mreq  mreq;
    VOS_ERR_T       result = VOS_NO_ERR;

    if (sock == (INT32)INVALID_SOCKET )
    {
        result = VOS_PARAM_ERR;
    }
    /*    Is this a multicast address?    */
    else if (IN_MULTICAST(mcAddress))
    {

        mreq.imr_multiaddr.s_addr   = vos_htonl(mcAddress);
        mreq.imr_interface.s_addr   = vos_htonl(ipAddress);

        {
            char    mcStr[16];
            char    ifStr[16];

            (void) strcpy_s(mcStr, sizeof(mcStr), inet_ntoa(mreq.imr_multiaddr));
            (void) strcpy_s(ifStr, sizeof(mcStr), inet_ntoa(mreq.imr_interface));
            vos_printLog(VOS_LOG_INFO, "leaving MC: %s on iface %s\n", mcStr, ifStr);
        }

        if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&mreq,
                       sizeof(mreq)) == SOCKET_ERROR)
        {
            int err = WSAGetLastError();

            err = err; /* for lint */
            vos_printLog(VOS_LOG_ERROR, "setsockopt() IP_DROP_MEMBERSHIP failed (Err: %d)\n", err);
            result = VOS_SOCK_ERR;
        }
        else
        {
            result = VOS_NO_ERR;
        }
    }
    else
    {
        result = VOS_PARAM_ERR;
    }

    return result;
}

/**********************************************************************************************************************/
/** Send UDP data.
 *  Send data to the supplied address and port.
 *
 *  @param[in]      sock            socket descriptor
 *  @param[in]      pBuffer         pointer to data to send
 *  @param[in,out]  pSize           IN: bytes to send, OUT: bytes sent
 *  @param[in]      ipAddress       destination IP
 *  @param[in]      port            destination port
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown, parameter error
 *  @retval         VOS_IO_ERR      data could not be sent
 *  @retval         VOS_BLOCK_ERR   Call would have blocked in blocking mode
 */

EXT_DECL VOS_ERR_T vos_sockSendUDP (
    INT32       sock,
    const UINT8 *pBuffer,
    UINT32      *pSize,
    UINT32      ipAddress,
    UINT16      port)
{
    struct sockaddr_in destAddr;
    int sendSize    = 0;
    int size        = 0;
    int err         = 0;

    if ((sock == (INT32)INVALID_SOCKET)
        || (pBuffer == NULL)
        || (pSize == NULL))
    {
        return VOS_PARAM_ERR;
    }

    size    = *pSize;
    *pSize  = 0;

    /*      We send UDP packets to the address  */
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family         = AF_INET;
    destAddr.sin_addr.s_addr    = vos_htonl(ipAddress);
    destAddr.sin_port           = vos_htons(port);

    do
    {
        sendSize = sendto(sock,
                          (const char *)pBuffer,
                          size,
                          0,
                          (struct sockaddr *) &destAddr,
                          sizeof(destAddr));
        err = WSAGetLastError();

        if (sendSize >= 0)
        {
            *pSize += sendSize;
        }

        if (sendSize == SOCKET_ERROR && err == WSAEWOULDBLOCK)
        {
            return VOS_BLOCK_ERR;
        }
    }
    while (sendSize == SOCKET_ERROR && err == WSAEINTR);

    if (sendSize == SOCKET_ERROR)
    {
        vos_printLog(VOS_LOG_ERROR, "sendto() to %s:%u failed (Err: %d)\n",
                     inet_ntoa(destAddr.sin_addr), port, err);
        return VOS_IO_ERR;
    }
    return VOS_NO_ERR;
}



/**********************************************************************************************************************/
/** Receive UDP data.
 *  The caller must provide a sufficient sized buffer. If the supplied buffer is smaller than the bytes received, *pSize
 *  will reflect the number of copied bytes and the call should be repeated until *pSize is 0 (zero).
 *  If the socket was created in blocking-mode (default), then this call will block and will only return if data has
 *  been received or the socket was closed or an error occured.
 *  If called in non-blocking mode, and no data is available, VOS_NODATA_ERR will be returned.
 *  If pointers are provided, source IP, source port and destination IP will be reported on return.
 *
 *  @param[in]      sock            socket descriptor
 *  @param[out]     pBuffer         pointer to applications data buffer
 *  @param[in,out]  pSize           pointer to the received data size
 *  @param[out]     pSrcIPAddr      pointer to source IP
 *  @param[out]     pSrcIPPort      pointer to source port
 *  @param[out]     pDstIPAddr      pointer to dest IP
 *  @param[in]      peek            if true, leave data in queue
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown, parameter error
 *  @retval         VOS_IO_ERR      data could not be read
 *  @retval         VOS_NODATA_ERR  no data
 *  @retval         VOS_BLOCK_ERR   Call would have blocked in blocking mode
 */

EXT_DECL VOS_ERR_T vos_sockReceiveUDP (
    INT32   sock,
    UINT8   *pBuffer,
    UINT32  *pSize,
    UINT32  *pSrcIPAddr,
    UINT16  *pSrcIPPort,
    UINT32  *pDstIPAddr,
    BOOL8   peek)
{
    struct sockaddr_in  srcAddr;
    INT32 rcvSize   = 0;
    int err         = 0;
    char controlBuffer[64];                  /* buffer for destination address record */
    struct iovec        wsabuf;
    struct msghdr       Msg;

    if (sock == (INT32)INVALID_SOCKET || pBuffer == NULL || pSize == NULL)
    {
        return VOS_PARAM_ERR;
    }

    memset(&srcAddr, 0, sizeof (struct sockaddr_in));
    memset(&controlBuffer [0], 0, CMSGSize);

    /* fill the scatter/gather list with our data buffer */
    wsabuf.iov_base = (CHAR *) pBuffer;
    wsabuf.iov_len  = *pSize;

    Msg.msg_name        = (struct sockaddr *) &srcAddr;
    Msg.msg_namelen     = sizeof (struct sockaddr_in);
    Msg.msg_iov         = &wsabuf;
    Msg.msg_iovlen      = 1;
    Msg.msg_control     = &controlBuffer [0];
    Msg.msg_controllen  = sizeof (controlBuffer);
    Msg.msg_flags       = 0;

    *pSize = 0;
    memset(&srcAddr, 0, sizeof(srcAddr));

    /* fill the msg block for recvmsg */
    do
    {
        rcvSize = recvmsg(sock, &Msg, (peek != 0) ? MSG_PEEK : 0);

        if (rcvSize != SOCKET_ERROR)
        {

            if (pDstIPAddr != NULL)
            {
                WSACMSGHDR *pCMsgHdr;

                pCMsgHdr = (WSACMSGHDR *) WSA_CMSG_FIRSTHDR(&Msg);
                if (pCMsgHdr &&
                    pCMsgHdr->cmsg_type == IP_PKTINFO)
                {
                    struct in_pktinfo *pPktInfo = (struct in_pktinfo *) WSA_CMSG_DATA(pCMsgHdr);
                    *pDstIPAddr = (UINT32) vos_ntohl(pPktInfo->ipi_addr.S_un.S_addr);
                    /* vos_printLog(VOS_LOG_DBG, "udp message dest IP: %s\n", vos_ipDotted(*pDstIPAddr)); */
                }
            }
            if (pSrcIPAddr != NULL)
            {
                *pSrcIPAddr = (UINT32) vos_ntohl(srcAddr.sin_addr.s_addr);
                /* vos_printLog(VOS_LOG_INFO, "recvmsg found %d bytes from IP address %x to \n", rcvSize, *pSrcIPAddr);
                  */
            }
            if (NULL != pSrcIPPort)
            {
                *pSrcIPPort = (UINT32) vos_ntohs(srcAddr.sin_port);
            }
        }
        else
        {
            err = WSAGetLastError();
        }

        if (rcvSize == SOCKET_ERROR && err == WSAEWOULDBLOCK)
        {
            return VOS_BLOCK_ERR;
        }
    }
    while (rcvSize < 0 && err == WSAEINTR);

    if (rcvSize == SOCKET_ERROR)
    {
        if (err == WSAECONNRESET)
        {
            /* ICMP port unreachable received (result of previous send), treat this as no error */
            return VOS_NO_ERR;
        }
        else
        {
            vos_printLog(VOS_LOG_ERROR, "recvfrom() failed (Err: %d)\n", err);
            return VOS_IO_ERR;
        }
    }
    else if (rcvSize == 0)
    {
        return VOS_NODATA_ERR;
    }
    else
    {
        *pSize = rcvSize;
        return VOS_NO_ERR;
    }

}

/**********************************************************************************************************************/
/** Bind a socket to an address and port.
 *
 *
 *  @param[in]      sock            socket descriptor
 *  @param[in]      ipAddress       source IP to receive on, 0 for any
 *  @param[in]      port            port to receive on, 17224 for PD
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown, parameter error
 *  @retval         VOS_IO_ERR      Input/Output error
 *  @retval         VOS_MEM_ERR     resource error
 */

EXT_DECL VOS_ERR_T vos_sockBind (
    INT32   sock,
    UINT32  ipAddress,
    UINT16  port)
{
    struct sockaddr_in srcAddress;

    if (sock == (INT32)INVALID_SOCKET )
    {
        return VOS_PARAM_ERR;
    }

    /*  Never bind to multicast address  */
    if (vos_isMulticast(ipAddress))
    {
        ipAddress = VOS_INADDR_ANY;
    }

    /*  Allow the socket to be bound to an address and port
        that is already in use    */

    memset((char *)&srcAddress, 0, sizeof(srcAddress));
    srcAddress.sin_family       = AF_INET;
    srcAddress.sin_addr.s_addr  = vos_htonl(ipAddress);
    srcAddress.sin_port         = vos_htons(port);

    vos_printLog(VOS_LOG_INFO, "binding to: %s:%hu\n",
                 inet_ntoa(srcAddress.sin_addr), port);

    /*  Try to bind the socket to the PD port.    */
    if (bind(sock, (struct sockaddr *)&srcAddress, sizeof(srcAddress)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();

        err = err; /* for lint */
        vos_printLog(VOS_LOG_ERROR, "bind() failed (Err: %d)\n", err);
        return VOS_SOCK_ERR;
    }
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Listen for incoming connections.
 *
 *  @param[in]      sock            socket descriptor
 *  @param[in]      backlog         maximum connection attempts if system is busy
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown, parameter error
 *  @retval         VOS_IO_ERR      Input/Output error
 *  @retval         VOS_MEM_ERR     resource error
 */

EXT_DECL VOS_ERR_T vos_sockListen (
    INT32   sock,
    UINT32  backlog)
{
    if (sock == (INT32)INVALID_SOCKET )
    {
        return VOS_PARAM_ERR;
    }
    if (listen(sock, (int) backlog) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();

        err = err; /* for lint */
        vos_printLog(VOS_LOG_ERROR, "listen() failed (Err: %d)\n", err);
        return VOS_IO_ERR;
    }
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Accept an incoming TCP connection.
 *  Accept incoming connections on the provided socket. May block and will return a new socket descriptor when
 *    accepting a connection. The original socket *pSock, remains open.
 *
 *
 *  @param[in]      sock            Socket descriptor
 *  @param[out]     pSock           Pointer to socket descriptor, on exit new socket
 *  @param[out]     pIPAddress      source IP to receive on, 0 for any
 *  @param[out]     pPort           port to receive on, 17224 for PD
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   NULL parameter, parameter error
 *  @retval         VOS_UNKNOWN_ERR sock descriptor unknown error
 */

EXT_DECL VOS_ERR_T vos_sockAccept (
    INT32   sock,
    INT32   *pSock,
    UINT32  *pIPAddress,
    UINT16  *pPort)
{
    struct sockaddr_in srcAddress;
    int connFd = SOCKET_ERROR;

    if (pSock == NULL || pIPAddress == NULL || pPort == NULL)
    {
        return VOS_PARAM_ERR;
    }

    memset((char *)&srcAddress, 0, sizeof(srcAddress));

    srcAddress.sin_family       = AF_INET;
    srcAddress.sin_addr.s_addr  = vos_htonl(*pIPAddress);
    srcAddress.sin_port         = vos_htons(*pPort);

    for (;; )
    {
        int sockLen = sizeof(srcAddress);
        connFd = accept(sock, (struct sockaddr *) &srcAddress, &sockLen);
        if (connFd < 0)
        {
            int err = WSAGetLastError();

            switch (err)
            {
               /*Accept return -1 and err = EWOULDBLOCK,
               when there is no more connection requests.*/
               case WSAEWOULDBLOCK:
               {
                   *pSock = connFd;
                   return VOS_NO_ERR;
               }
               case WSAEINTR:         break;
               case WSAECONNABORTED:  break;
#if defined (WSAEPROTO)
               case WSAEPROTO:        break;
#endif
               default:
               {
                   vos_printLog(VOS_LOG_ERROR, "accept() failed (socket: %d, err: %d)", *pSock, err);
                   return VOS_UNKNOWN_ERR;
               }
            }
        }
        else
        {
            *pIPAddress = vos_htonl(srcAddress.sin_addr.s_addr);
            *pPort      = vos_htons(srcAddress.sin_port);
            *pSock      = connFd;
            break;         /* success */
        }
    }
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Open a TCP connection.
 *
 *
 *  @param[in]      sock            socket descriptor
 *  @param[in]      ipAddress       destination IP
 *  @param[in]      port            destination port
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown, parameter error
 *  @retval         VOS_IO_ERR      Input/Output error
 *  @retval         VOS_MEM_ERR     resource error
 */

EXT_DECL VOS_ERR_T vos_sockConnect (
    INT32   sock,
    UINT32  ipAddress,
    UINT16  port)
{
    struct sockaddr_in dstAddress;

    if (sock == (INT32)INVALID_SOCKET )
    {
        return VOS_PARAM_ERR;
    }

    memset((char *)&dstAddress, 0, sizeof(dstAddress));
    dstAddress.sin_family       = AF_INET;
    dstAddress.sin_addr.s_addr  = vos_htonl(ipAddress);
    dstAddress.sin_port         = vos_htons(port);

    if (connect((SOCKET)sock, (const struct sockaddr *) &dstAddress,
                sizeof(dstAddress)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();

        if ((err == WSAEINPROGRESS)
            || (err == WSAEWOULDBLOCK)
            || (err == WSAEALREADY))
        {
            return VOS_BLOCK_ERR;
        }
        else if (err != WSAEISCONN)
        {
            vos_printLog(VOS_LOG_WARNING, "connect() failed (Err: %d)\n", err);
            return VOS_IO_ERR;
        }
    }
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Send TCP data.
 *  Send data to the supplied address and port.
 *
 *  @param[in]      sock            socket descriptor
 *  @param[in]      pBuffer         pointer to data to send
 *  @param[in,out]  pSize           IN: bytes to send, OUT: bytes sent
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown, parameter error
 *  @retval         VOS_IO_ERR      data could not be sent
 *  @retval         VOS_NOCONN_ERR  no TCP connection
 *  @retval         VOS_BLOCK_ERR   Call would have blocked in blocking mode
 */

EXT_DECL VOS_ERR_T vos_sockSendTCP (
    INT32       sock,
    const UINT8 *pBuffer,
    UINT32      *pSize)
{
    int sendSize = 0;
    int bufferSize;
    int err = 0;

    if ((sock == (INT32)INVALID_SOCKET)
        || (pBuffer == NULL)
        || (pSize == NULL))
    {
        return VOS_PARAM_ERR;
    }

    bufferSize  = (int) *pSize;
    *pSize      = 0;

    /*    Keep on sending until we got rid of all data or we received an unrecoverable error    */
    do
    {
        sendSize    = send((SOCKET)sock, (char *)pBuffer, bufferSize, 0);
        err         = WSAGetLastError();

        if (sendSize >= 0)
        {
            bufferSize  -= sendSize;
            pBuffer     += sendSize;
            *pSize      += sendSize;
        }

        if (sendSize == -1 && err == WSAEWOULDBLOCK)
        {
            return VOS_BLOCK_ERR;
        }
    }
    while (bufferSize && !(sendSize == -1 && err != WSAEINTR));

    if (sendSize == -1)
    {
        vos_printLog(VOS_LOG_WARNING, "send() failed (Err: %d)\n", err);

        if (err == WSAENOTCONN)
        {
            return VOS_NOCONN_ERR;
        }
        else
        {
            return VOS_IO_ERR;
        }
    }
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Receive TCP data.
 *  The caller must provide a sufficient sized buffer. If the supplied buffer is smaller than the bytes received, *pSize
 *    will reflect the number of copied bytes and the call should be repeated until *pSize is 0 (zero).
 *    If the socket was created in blocking-mode (default), then this call will block and will only return if data has
 *    been received or the socket was closed or an error occured.
 *    If called in non-blocking mode, and no data is available, VOS_NODATA_ERR will be returned.
 *
 *  @param[in]      sock            socket descriptor
 *  @param[out]     pBuffer         pointer to applications data buffer
 *  @param[in,out]  pSize           pointer to the received data size
 *
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   sock descriptor unknown, parameter error
 *  @retval         VOS_IO_ERR      data could not be read
 *  @retval         VOS_NODATA_ERR  no data
 *  @retval         VOS_BLOCK_ERR   call would have blocked in blocking mode
 */

EXT_DECL VOS_ERR_T vos_sockReceiveTCP (
    INT32   sock,
    UINT8   *pBuffer,
    UINT32  *pSize)
{
    int rcvSize     = 0;
    int bufferSize  = (int) *pSize;
    int err         = 0;

    *pSize = 0;

    if (sock == (INT32)INVALID_SOCKET || pBuffer == NULL || pSize == NULL)
    {
        return VOS_PARAM_ERR;
    }

    /* Keep on sending until we got rid of all data or we received an unrecoverable error    */
    do
    {
        rcvSize = recv((SOCKET)sock, (char *)pBuffer, bufferSize, 0);
        err     = WSAGetLastError();

        if (rcvSize > 0)
        {
            bufferSize  -= rcvSize;
            pBuffer     += rcvSize;
            *pSize      += rcvSize;
        }

        if (rcvSize == -1 && err == WSAEWOULDBLOCK)
        {
            if (*pSize == 0)
            {
                return VOS_BLOCK_ERR;
            }
            else
            {
                return VOS_NO_ERR;
            }
        }
    }
    while ((bufferSize > 0 && rcvSize > 0) || (rcvSize == -1 && err == WSAEINTR));

    if ((rcvSize == SOCKET_ERROR) && !(err == WSAEMSGSIZE))
    {
        if (err == WSAECONNRESET)
        {
            return VOS_NODATA_ERR;
        }
        else
        {
            vos_printLog(VOS_LOG_WARNING, "receive() failed (Err: %d)\n", err);
            return VOS_IO_ERR;
        }
    }
    else if (*pSize == 0)
    {
        if (err == WSAEMSGSIZE)
        {
            return VOS_MEM_ERR;
        }
        else
        {
            return VOS_NODATA_ERR;
        }
    }
    else
    {
        return VOS_NO_ERR;
    }
}


/**********************************************************************************************************************/
/** Set Using Multicast I/F
 *
 *  @param[in]      sock                socket descriptor
 *  @param[in]      mcIfAddress         using Multicast I/F Address
 *
 *  @retval         VOS_NO_ERR          no error
 *  @retval         VOS_PARAM_ERR       sock descriptor unknown, parameter error
 */
EXT_DECL VOS_ERR_T vos_sockSetMulticastIf (
    INT32   sock,
    UINT32  mcIfAddress)
{
    DWORD       optValue    = vos_htonl(mcIfAddress);
    VOS_ERR_T   result      = VOS_NO_ERR;

    if (sock == (INT32) INVALID_SOCKET)
    {
        result = VOS_PARAM_ERR;
    }
    else
    {
        if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (const char *) &optValue, sizeof(optValue)) == SOCKET_ERROR)
        {
            int err = WSAGetLastError();

            err = err; /* for lint */
            vos_printLog(VOS_LOG_WARNING, "setsockopt IP_MULTICAST_IF failed (Err: %d)\n", err);
            result = VOS_SOCK_ERR;
        }
        else
        {
            result = VOS_NO_ERR;
        }
    }
    return result;
}


/**********************************************************************************************************************/
/** Determines the address to bind to since the behaviour in the different OS is different
 *  @param[in]      srcIP           IP to bind to (0 = any address)
 *  @param[in]      mcGroup         MC group to join (0 = do not join)
 *  @param[in]      rcvMostly       primarily used for receiving (tbd: bind on sender, too?)
 *
 *  @retval         Address to bind to
 */
EXT_DECL VOS_IP4_ADDR_T vos_determineBindAddr ( VOS_IP4_ADDR_T  srcIP,
                                                VOS_IP4_ADDR_T  mcGroup,
                                                VOS_IP4_ADDR_T  rcvMostly)
{
    return srcIP;
}