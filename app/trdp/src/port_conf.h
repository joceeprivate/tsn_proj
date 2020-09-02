
#ifndef PORT_CONF_H
#define PORT_CONF_H

/***********************************************************************************************************************
 * INCLUDES
 */

#include "trdp_private.h"

/***********************************************************************************************************************
 * DEFINES
 */

/***********************************************************************************************************************
 * TYPEDEFS
 */

/** Queue element for PORT receive    */
typedef struct PORT_ELE
{
    struct PORT_ELE       *pNext;                 /**< pointer to next element or NULL                        */
    UINT16              index;    
    TRDP_APP_SESSION_T   appHandle;
    TRDP_SUB_T           subHandle;
    UINT32              cycle;
    UINT32              timeout;
    TRDP_TIME_T         lastGetTime;
} PORT_ELE_T, *PORT_HANDLE;

/***********************************************************************************************************************
 * FUNCTIONS
 */

TRDP_ERR_T port_subscribe (
    PORT_HANDLE         *pPortList,
    UINT16              index,
    TRDP_APP_SESSION_T  appHandle,    
    UINT32              comId,    
    TRDP_IP_ADDR_T      srcIpAddr,
    TRDP_IP_ADDR_T      destIpAddr,
    UINT32              cycle,
    UINT32              timeout); 

#endif /* PORT_CONF_H */

