/**********************************************************************************************************************/
/**
 * @file            posix/vos_private.h
 *
 * @brief           Private definitions for the OS abstraction layer
 *
 * @details
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Bernd Loehr, NewTec GmbH
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2013. All rights reserved.
 *
 * $Id: vos_private.h 1813 2018-11-26 14:03:32Z bloehr $
 *
 */

#ifndef VOS_PRIVATE_H
#define VOS_PRIVATE_H

/***********************************************************************************************************************
 * INCLUDES
 */

#include <pthread.h>
#include <sys/types.h>

#include "vos_types.h"
#include "vos_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************************************************
 * DEFINES
 */

/* The VOS version can be predefined as CFLAG   */
#ifndef VOS_VERSION
#define VOS_VERSION            1u
#define VOS_RELEASE            0u
#define VOS_UPDATE             3u
#define VOS_EVOLUTION          0u
#endif

struct VOS_MUTEX
{
    UINT32          magicNo;
    pthread_mutex_t mutexId;
};

struct VOS_SHRD
{
    INT32   fd;                     /* File descriptor */
    CHAR8   *sharedMemoryName;      /* shared memory Name */
};

VOS_ERR_T   vos_mutexLocalCreate (struct VOS_MUTEX *pMutex);
void        vos_mutexLocalDelete (struct VOS_MUTEX *pMutex);

#if (((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE) || __APPLE__)
#   define STRING_ERR(pStrBuf)  (void)strerror_r(errno, pStrBuf, VOS_MAX_ERR_STR_SIZE);
#elif INTEGRITY
#   define STRING_ERR(pStrBuf)                                 \
    {                                                          \
        char *pStr = strerror(errno);                          \
        if (pStr != NULL)                                      \
        {                                                      \
            strncpy(pStrBuf, pStr, VOS_MAX_ERR_STR_SIZE);      \
        }                                                      \
    }
#else
#   define STRING_ERR(pStrBuf)                                         \
    {                                                                  \
        char *pStr = strerror_r(errno, pStrBuf, VOS_MAX_ERR_STR_SIZE); \
        if (pStr != NULL)                                              \
        {                                                              \
            strncpy(pStrBuf, pStr, VOS_MAX_ERR_STR_SIZE);              \
        }                                                              \
    }
#endif

#ifdef __cplusplus
}
#endif

#endif /* VOS_UTILS_H */
