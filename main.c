// mostly copied from scipshell.c

// from SCIP installation (should be in /usr/include or similar)
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/scipshell.h"

// locally
#include "event_primalstall.h"

int main(int argc, char** argv)
{
    // create normal SCIP
    SCIP* scip = NULL;
    SCIP_CALL(SCIPcreate(&scip));
    SCIP_CALL(SCIPincludeDefaultPlugins(scip));

    // add our new event handler
    SCIP_CALL(SCIPincludeEventHdlrPrimalstall(scip));

    // run normal SCIP shell
    SCIP_CALL(SCIPprocessShellArguments(scip, argc, argv, NULL));

    // clean up
    SCIP_CALL(SCIPfree(&scip));
    return 0;
}
