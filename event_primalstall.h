#ifndef __SCIP_EVENT_PRIMALSTALL_H__
#define __SCIP_EVENT_PRIMALSTALL_H__


#include "scip/scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates event handler for primalstall event */
EXTERN
SCIP_RETCODE SCIPincludeEventHdlrPrimalstall(
    SCIP*                 scip                /**< SCIP data structure */
    );

#ifdef __cplusplus
}
#endif

#endif
