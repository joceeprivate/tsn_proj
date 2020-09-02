
#ifndef TADMA_CONF_H
#define TADMA_CONF_H

/***********************************************************************************************************************
 * INCLUDES
 */

#include <string.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netinet/in.h>

#include "vos_types.h"

/***********************************************************************************************************************
 * DEFINES
 */

/***********************************************************************************************************************
 * TYPEDEFS
 */

typedef enum
{
    SIOCCHIOCTL = SIOCDEVPRIVATE,
    SIOC_GET_SCHED,
    SIOC_PREEMPTION_CFG,
    SIOC_PREEMPTION_CTRL,
    SIOC_PREEMPTION_STS,
    SIOC_PREEMPTION_COUNTER,
    SIOC_QBU_USER_OVERRIDE,
    SIOC_QBU_STS,
    SIOC_TADMA_STR_ADD,
    SIOC_TADMA_PROG_ALL,
    SIOC_TADMA_STR_FLUSH,
    SIOC_PREEMPTION_RECEIVE,
    SIOC_TADMA_OFF,
} axienet_tsn_ioctl;

typedef struct
{
    UINT32 dstIp;
    UINT32 comId;
    UINT32 trigger;
    UINT8 count;
    BOOL8 start;
} tadma_stream;

/***********************************************************************************************************************
 * FUNCTIONS
 */

static inline int tadma_streamFlush(int socket, const char *interface)
{
    struct ifreq req;

    strcpy(req.ifr_name, interface);

    return ioctl(socket, SIOC_TADMA_STR_FLUSH, &req);
}

static inline int tadma_streamAdd(INT32 socket, 
                            const char *interface,
                            UINT32      dstIp,
                            UINT32      comId,
                            UINT32      trigger,
                            UINT8       count,
                            BOOL8       start)
{
    struct ifreq req;
    tadma_stream stream;

    stream.dstIp = dstIp;
    stream.comId = comId;
    stream.trigger = trigger;
    stream.count = count;
    stream.start = start;    

    strcpy(req.ifr_name, interface);
    req.ifr_data = (void *)&stream;

    return ioctl(socket, SIOC_TADMA_STR_ADD, &req);    
}

static inline int tadma_streamProgram(INT32 socket, const char *interface)
{
    struct ifreq req;

    strcpy(req.ifr_name, interface);

    return ioctl(socket, SIOC_TADMA_PROG_ALL, &req);  
}


#endif /* TADMA_CONF_H */

