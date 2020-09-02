
/***********************************************************************************************************************
 * INCLUDES
 */

#include "thread_cmd.h"

#include <stdio.h>
#include <sys/select.h>

#include "port_conf.h"
#include "trdp_if_light.h"

 /***********************************************************************************************************************
  * DEFINES
  */

  /***********************************************************************************************************************
   * TYPEDEFS
   */

   /***********************************************************************************************************************
    * FUNCTIONS
    */

void thread_process_pdRecv(void* handle)
{
    TRDP_APP_SESSION_T  appHandle;
    appHandle = (TRDP_APP_SESSION_T)handle;

    TRDP_FDS_T          rfds;
    INT32               noDesc = 0;
    TRDP_TIME_T         tv, now;
    const TRDP_TIME_T   max_tv  ={ 0, 100000 };
    const TRDP_TIME_T   min_tv  ={ 0, 1000 };
    int rv = 0;

    printf("RECV Process started!!\n");

    while (1)
    {
        FD_ZERO(&rfds);

        tlc_getInterval(appHandle, &tv, &rfds, &noDesc);

        if (vos_cmpTime(&tv, &max_tv) > 0)
        {
            tv = max_tv;
        }
        else if (vos_cmpTime(&tv, &min_tv) < 0)
        {
            tv = min_tv;
        }

        rv = vos_select(noDesc + 1, &rfds, NULL, NULL, &tv);

        tlc_process_recv(appHandle, &rfds, &rv);
    }

    tlc_closeSession(appHandle);
}

void thread_process_pdSend(void* handle)
{
    TRDP_APP_SESSION_T  appHandle;
    appHandle = (TRDP_APP_SESSION_T)handle;

    //TRDP_FDS_T          rfds;
    //INT32               noDesc = 0;
    TRDP_TIME_T         tv;
    const TRDP_TIME_T   max_tv  ={ 0, 100000 };
    const TRDP_TIME_T   min_tv  ={ 0, 1000 };
    int rv = 0;

    printf("SEND Process started!!\n");

    while (1)
    {

        tlc_getInterval(appHandle, &tv, NULL, NULL);

        if (vos_cmpTime(&tv, &max_tv) > 0)
        {
            tv = max_tv;
        }
        else if (vos_cmpTime(&tv, &min_tv) < 0)
        {
            tv = min_tv;
        }

        vos_threadDelay(tv.tv_usec);

        tlc_process_send(appHandle);
    }

    tlc_closeSession(appHandle);
}

void thread_process_pdGet(void* handle)
{
    PORT_HANDLE     iterPH;
    TRDP_TIME_T     now;

    UINT32          elapseTime;
    TRDP_PD_INFO_T  myPDInfo;
    UINT8           buffer[1432];
    UINT32          receivedSize = sizeof(buffer);
    TRDP_ERR_T      ret;

    static  UINT32  lastSeqCnt[256];

    printf("PDGET Process started!!\n");

    while (1)
    {
        vos_getTime(&now);
        for (iterPH = (PORT_HANDLE) handle; iterPH->pNext != NULL; iterPH = iterPH->pNext)
        {
            elapseTime = 1000000 * (now.tv_sec - iterPH->lastGetTime.tv_sec) + now.tv_usec - iterPH->lastGetTime.tv_usec;
            if (elapseTime > 0.5 * iterPH->cycle)
            {
                ret = tlp_get(iterPH->appHandle,
                    iterPH->subHandle,
                    &myPDInfo,
                    buffer,
                    &receivedSize);

                if ((ret == TRDP_NO_ERR) && (receivedSize > 0))
                {
                    if (myPDInfo.seqCount && myPDInfo.seqCount > lastSeqCnt[iterPH->index] + 1)
                    {
                        printf("GET dropped : COMID = %d, seq = %d, last = %d\n", myPDInfo.comId, myPDInfo.seqCount, lastSeqCnt[iterPH->index]);
                    }
                    lastSeqCnt[iterPH->index] = myPDInfo.seqCount;
                }
                // else if (ret == TRDP_TIMEOUT_ERR)
                // {
                //     printf("Packet timed out\n");
                // }
                iterPH->lastGetTime.tv_sec = now.tv_sec;
                iterPH->lastGetTime.tv_usec = now.tv_usec;
            }
        }
        vos_threadDelay(5000);
    }
}
