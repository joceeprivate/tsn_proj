
/***********************************************************************************************************************
 * INCLUDES
 */

#include "port_conf.h"
#include "trdp_if_light.h"
#include "trdp_utils.h"

 /***********************************************************************************************************************
  * DEFINES
  */

  /***********************************************************************************************************************
   * TYPEDEFS
   */

   /***********************************************************************************************************************
    * FUNCTIONS
    */

TRDP_ERR_T port_subscribe(
    PORT_HANDLE         *pPortList,
    UINT16              index,
    TRDP_APP_SESSION_T  appHandle,
    UINT32              comId,
    TRDP_IP_ADDR_T      srcIpAddr,
    TRDP_IP_ADDR_T      destIpAddr,
    UINT32              cycle,
    UINT32              timeout)
{
    PORT_HANDLE         newPort, iterPH;
    TRDP_ERR_T          ret = TRDP_NO_ERR;

    if (appHandle == NULL)
    {
        return TRDP_PARAM_ERR;
    }

    if (!trdp_isValidSession(appHandle))
    {
        return TRDP_NOINIT_ERR;
    }


    /*    Reserve mutual access    */
    if (vos_mutexLock(appHandle->mutex) != VOS_NO_ERR)
    {
        return TRDP_NOINIT_ERR;
    }


    /*    Allocate a buffer for this port    */
    newPort = (PORT_HANDLE) vos_memAlloc(sizeof(PORT_ELE_T));

    if (newPort == NULL)
    {
        ret = TRDP_MEM_ERR;
    }
    else
    {
        newPort->index = index;
        newPort->appHandle = appHandle;
        newPort->cycle = cycle;
        newPort->timeout = timeout;
        newPort->pNext = NULL;
        newPort->lastGetTime.tv_sec = 0;
        newPort->lastGetTime.tv_usec = 0;

        ret = tlp_subscribe(appHandle,           /*    our application identifier            */
            &newPort->subHandle,                     /*    our subscription identifier           */
            NULL,                           /*    user reference                        */
            NULL,                           /*    callback functiom                     */
            comId,                          /*    ComID                                 */
            0,                              /*    etbTopoCnt: local consist only        */
            0,                              /*    opTopoCnt                             */
            srcIpAddr,                         /*    Source IP filter              */
            destIpAddr,                          /*    Default destination    (or MC Group)  */
            TRDP_FLAGS_DEFAULT,             /*    TRDP flags                            */
            timeout,             /*    Time out in us                        */
            TRDP_TO_SET_TO_ZERO             /*    delete invalid data on timeout        */
        );

        if (ret == TRDP_NO_ERR)
        {
            if (*pPortList == NULL)
            {
                *pPortList = newPort;
            }
            else
            {
                for (iterPH = *pPortList; iterPH->pNext != NULL; iterPH = iterPH->pNext)
                {
                    ;
                }
                iterPH->pNext = newPort;
            }
        }
        else
        {
            vos_memFree(newPort);
            vos_printLog(VOS_LOG_ERROR, "prep sub pd error, err code = %d\n", ret);
        }

    }

    if (vos_mutexUnlock(appHandle->mutex) != VOS_NO_ERR)
    {
        vos_printLogStr(VOS_LOG_INFO, "vos_mutexUnlock() failed\n");
    }

    return ret;

}

