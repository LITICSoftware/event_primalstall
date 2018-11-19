#include <math.h>
#include <string.h>

#include "event_primalstall.h"

/* #define DEBUGGING */
/* #define TESTING */

// debugging
#ifdef DEBUGGING
#define DBG printf
#else
#define DBG while(FALSE)printf
#endif

#define EVENTHDLR_NAME "primalstall"
#define EVENTHDLR_DESC ""

// default values for parameters
#define DEFAULT_ABSTOL      INFINITY
#define DEFAULT_RELTOL      0.01
#define DEFAULT_MINTIME     0.0
#define DEFAULT_MAXTIME     INFINITY
#define DEFAULT_FRACTIME    1.0

// initial values for state
#define DEFAULT_LASTSOLVAL  INFINITY // no solution yet
#define DEFAULT_LASTSOLTIME 0.0      // at beginning of solving time

// We want to know when an improving solution was found.
// We also want some regular event for time keeping (e.g., node solved).
#define EVENT_PRIMALSTALL (SCIP_EVENTTYPE_BESTSOLFOUND | SCIP_EVENTTYPE_NODESOLVED)

/*
 * Data structures
 */

/** event handler data */
struct SCIP_EventhdlrData
{
    // parameters
    SCIP_Real abstol;    // absolute tolerance for solution improvement
    SCIP_Real reltol;    // relative tolerance for solution improvement
    SCIP_Real mintime;   // minimum improvement time (s)
    SCIP_Real maxtime;   // maximum improvement time (s)
    SCIP_Real fractime;  // fraction of elapsed time

    // runtime state
    SCIP_Real lastsolval;  // objective value of last improvement
    SCIP_Real lastsoltime; // time when last improvement was found (s)
};


/*
 * Local methods
 */

static
SCIP_Bool significant(
    SCIP_Real old,
    SCIP_Real new,
    SCIP_Real abstol,
    SCIP_Real reltol,
    SCIP_OBJSENSE objsense)
{
    SCIP_Real improvement;
    DBG("  [significant]: old: %f new: %f, abs: %f, rel: %f, sense: %d - ",
        old, new, abstol, reltol, objsense);

    // new solution should be smaller with minimization,
    // and larger with maximization
    if (objsense == SCIP_OBJSENSE_MINIMIZE) {
        improvement = old - new;
    } else  {
        assert(objsense == SCIP_OBJSENSE_MAXIMIZE);
        improvement = new - old;
    }

    if (improvement <= 0.0) {
        DBG("no improvement\n");
        return FALSE; // no improvement at all
    } else if (improvement > abstol) {
        DBG("absolute improvement\n");
        return TRUE;  // enough absolute improvement
    } else if (old != 0.0) {
        DBG("relative improvement (old)\n");
        // improvement relative to old value
        return (improvement / abs(old)) > reltol;
    } else {
        DBG("relative improvement (new)\n");
        assert(new != 0.0);
        // improvement relative to new value
        return (improvement / abs(new)) > reltol;
    }
}

static
SCIP_Bool significantImprovement(
    SCIP_EVENTHDLRDATA* eventhdlrdata,
    SCIP_Real newsolval,
    SCIP_OBJSENSE objsense)
{
    assert(eventhdlrdata != NULL);
    SCIP_Real lastsolval = eventhdlrdata->lastsolval;

    if (lastsolval == INFINITY) {
        return TRUE; // first solution
    }

    return significant(lastsolval, newsolval, eventhdlrdata->abstol,
                       eventhdlrdata->reltol, objsense);
}

static
SCIP_Bool shouldStop(
    SCIP_Real minimprove,
    SCIP_Real maximprove,
    SCIP_Real fractotal,
    SCIP_Real lastsol,
    SCIP_Real current)
{
    SCIP_Real improve = current - lastsol;
    if(improve <= minimprove) {
        return FALSE;
    } else if (improve > maximprove) {
        printf("primalstall: interrupting solving (maximum improvement time exceeded)\n");
        return TRUE;
    } else if (improve > fractotal * current) {
        printf("primalstall: interrupting solving (fractional improvement time exceeded)\n");
        return TRUE;
    }
    return FALSE;
}

static
SCIP_Bool timeIsUp(
    SCIP_EVENTHDLRDATA* eventhdlrdata,
    SCIP_Real currenttime)
{
    assert(eventhdlrdata != NULL);

    return shouldStop(eventhdlrdata->mintime,
                      eventhdlrdata->maxtime,
                      eventhdlrdata->fractime,
                      eventhdlrdata->lastsoltime,
                      currenttime);
}


/*
 * Unit tests (from http://www.jera.com/techinfo/jtns/jtn002.html)
 */
#ifdef TESTING

#define mu_assert(message, test) do {        \
    if (!(test))                             \
    {                                        \
        printf("    failed: %s\n", message); \
        assert(test);                        \
    }                                        \
} while (0)


static
void runTests()
{
    printf("[ primalstall ] running tests...\n");

    // min: significant improvements (none)
    mu_assert("min: worse value (+,+)", !significant(2.0, 3.0, 0.0, 0.0, +1));
    mu_assert("min: worse value (0,+)", !significant(0.0, 2.0, 0.0, 0.0, +1));
    mu_assert("min: worse value (-,+)", !significant(-1.0, 1.0, 0.0, 0.0, +1));
    mu_assert("min: worse value (-,0)", !significant(-2.0, 0.0, 0.0, 0.0, +1));
    mu_assert("min: worse value (-,-)", !significant(-3.0, -2.0, 0.0, 0.0, +1));
    mu_assert("min: same value (+,+)", !significant(2.0, 2.0, 0.0, 0.0, +1));
    mu_assert("min: same value (0,0)", !significant(0.0, 0.0, 0.0, 0.0, +1));
    mu_assert("min: same value (-,-)", !significant(-2.0, -2.0, 0.0, 0.0, +1));

    // min: significant improvements (relative)
    mu_assert("min: big rel impr (+,+)", significant(10.0, 1.0, INFINITY, 0.5, +1));
    mu_assert("min: big rel impr (+,0)", significant(1.0, 0.0, INFINITY, 0.5, +1));
    mu_assert("min: big rel impr (+,-)", significant(1.0, -1.0, INFINITY, 0.5, +1));
    mu_assert("min: big rel impr (0,-)", significant(0.0, -1.0, INFINITY, 0.5, +1));
    mu_assert("min: big rel impr (-,-)", significant(-1.0, -10.0, INFINITY, 0.5, +1));

    mu_assert("min: small rel impr (+,+)",  significant(1.003, 1.001, INFINITY, 0.001, +1));
    mu_assert("min: small rel impr (+,+)", !significant(1.003, 1.001, INFINITY, 0.01, +1));
    mu_assert("min: small rel impr (-,-)",  significant(-1.001, -1.003, INFINITY, 0.001, +1));
    mu_assert("min: small rel impr (-,-)", !significant(-1.001, -1.003, INFINITY, 0.01, +1));

    // min: significant improvements (absolute)
    mu_assert("min: big abs impr (+,+)", significant(10.0, 1.0, 1.0, INFINITY, +1));
    mu_assert("min: big abs impr (+,0)", significant(2.0, 0.0, 1.0, INFINITY, +1));
    mu_assert("min: big abs impr (+,-)", significant(1.0, -1.0, 1.0, INFINITY, +1));
    mu_assert("min: big abs impr (0,-)", significant(0.0, -2.0, 1.0, INFINITY, +1));
    mu_assert("min: big abs impr (-,-)", significant(-1.0, -10.0, 1.0, INFINITY, +1));

    mu_assert("min: small abs impr (+,+)",  significant(1.003, 1.001, 0.001, INFINITY, +1));
    mu_assert("min: small abs impr (+,+)", !significant(1.003, 1.001, 0.01, INFINITY, +1));
    mu_assert("min: small abs impr (-,-)",  significant(-1.001, -1.003, 0.001, INFINITY, +1));
    mu_assert("min: small abs impr (-,-)", !significant(-1.001, -1.003, 0.01, INFINITY, +1));

    // max: significant improvements (none)
    mu_assert("max: worse value (+,+)", !significant(3.0, 2.0, 0.0, 0.0, -1));
    mu_assert("max: worse value (+,0)", !significant(2.0, 0.0, 0.0, 0.0, -1));
    mu_assert("max: worse value (+,-)", !significant(1.0, -1.0, 0.0, 0.0, -1));
    mu_assert("max: worse value (0,-)", !significant(0.0, -2.0, 0.0, 0.0, -1));
    mu_assert("max: worse value (-,-)", !significant(-2.0, -3.0, 0.0, 0.0, -1));
    mu_assert("max: same value (+,+)", !significant(2.0, 2.0, 0.0, 0.0, -1));
    mu_assert("max: same value (0,0)", !significant(0.0, 0.0, 0.0, 0.0, -1));
    mu_assert("max: same value (-,-)", !significant(-2.0, -2.0, 0.0, 0.0, -1));

    // max: significant improvements (relative)
    mu_assert("max: big rel impr (+,+)", significant(1.0, 10.0, INFINITY, 0.5, -1));
    mu_assert("max: big rel impr (0,+)", significant(0.0, 1.0, INFINITY, 0.5, -1));
    mu_assert("max: big rel impr (-,+)", significant(-1.0, 1.0, INFINITY, 0.5, -1));
    mu_assert("max: big rel impr (-,0)", significant(-1.0, 0.0, INFINITY, 0.5, -1));
    mu_assert("max: big rel impr (-,-)", significant(-10.0, -1.0, INFINITY, 0.5, -1));

    mu_assert("max: small rel impr (+,+)",  significant(1.001, 1.003, INFINITY, 0.001, -1));
    mu_assert("max: small rel impr (+,+)", !significant(1.001, 1.003, INFINITY, 0.01, -1));
    mu_assert("max: small rel impr (-,-)",  significant(-1.003, -1.001, INFINITY, 0.001, -1));
    mu_assert("max: small rel impr (-,-)", !significant(-1.003, -1.001, INFINITY, 0.01, -1));

    // max: significant improvements (absolute)
    mu_assert("max: big abs impr (+,+)", significant(1.0, 10.0, 1.0, INFINITY, -1));
    mu_assert("max: big abs impr (0,+)", significant(0.0, 2.0, 1.0, INFINITY, -1));
    mu_assert("max: big abs impr (-,+)", significant(-1.0, 1.0, 1.0, INFINITY, -1));
    mu_assert("max: big abs impr (-,0)", significant(-2.0, 0.0, 1.0, INFINITY, -1));
    mu_assert("max: big abs impr (-,-)", significant(-10.0, -1.0, 1.0, INFINITY, -1));

    mu_assert("max: small abs impr (+,+)",  significant(1.001, 1.003, 0.001, INFINITY, -1));
    mu_assert("max: small abs impr (+,+)", !significant(1.001, 1.003, 0.01, INFINITY, -1));
    mu_assert("max: small abs impr (-,-)",  significant(-1.003, -1.001, 0.001, INFINITY, -1));
    mu_assert("max: small abs impr (-,-)", !significant(-1.003, -1.001, 0.01, INFINITY, -1));

    // shouldStop
    mu_assert("default (never stop)", !shouldStop(0.0, INFINITY, 1.0, 0.0, 0.0));
    mu_assert("default (never stop)", !shouldStop(0.0, INFINITY, 1.0, 0.0, 1.0));
    mu_assert("default (never stop)", !shouldStop(0.0, INFINITY, 1.0, 0.0, INFINITY));

    mu_assert("T25 (not at start)" , !shouldStop(0.0, INFINITY, 0.25, 0.0, 0.0));
    mu_assert("T25 (stop early)"   ,  shouldStop(0.0, INFINITY, 0.25, 0.0, 1.0));
    mu_assert("T25 (< mintime)"    , !shouldStop(2.0, INFINITY, 0.25, 0.0, 1.0));
    mu_assert("T25 (= mintime)"    , !shouldStop(2.0, INFINITY, 0.25, 0.0, 2.0));
    mu_assert("T25 (> mintime)"    ,  shouldStop(2.0, INFINITY, 0.25, 0.0, 3.0));

    mu_assert("T25+sol (not at start)" , !shouldStop(0.0, INFINITY, 0.25, 10.0, 10.0));
    mu_assert("T25+sol (too early)"    , !shouldStop(0.0, INFINITY, 0.25, 10.0, 11.0));
    mu_assert("T25+sol (too late)"     ,  shouldStop(0.0, INFINITY, 0.25, 10.0, 14.0));
    mu_assert("T25+sol (< mintime)"    , !shouldStop(5.0, INFINITY, 0.25, 10.0, 15.0));
    mu_assert("T25+sol (= mintime)"    , !shouldStop(5.0, INFINITY, 0.25, 10.0, 15.0));
    mu_assert("T25+sol (> mintime)"    ,  shouldStop(5.0, INFINITY, 0.25, 10.0, 16.0));

    mu_assert("< maxtime", !shouldStop(0.0, 1.0, 1.0, 0.0, 0.5));
    mu_assert("= maxtime", !shouldStop(0.0, 1.0, 1.0, 0.0, 1.0));
    mu_assert("> maxtime",  shouldStop(0.0, 1.0, 1.0, 0.0, 1.5));

    printf("[ primalstall ] all passed!\n");
}

#endif

/*
 * Callback methods of event handler
 */

/** copy method for event handler plugins (called when SCIP copies plugins) */
static
SCIP_DECL_EVENTCOPY(eventCopyPrimalstall)
{
    assert(scip != NULL);
    assert(eventhdlr != NULL);
    assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);

    /* call inclusion method of event handler */
    SCIP_CALL(SCIPincludeEventHdlrPrimalstall(scip));

    return SCIP_OKAY;
}

/** destructor of event handler to free user data (called when SCIP is exiting) */
static
SCIP_DECL_EVENTFREE(eventFreePrimalstall)
{
    SCIP_EVENTHDLRDATA* eventhdlrdata;

    assert(scip != NULL);
    assert(eventhdlr != NULL);
    assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);

    eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);
    assert(eventhdlrdata != NULL);

    SCIPfreeBlockMemory(scip, &eventhdlrdata);
    SCIPeventhdlrSetData(eventhdlr, NULL);

    return SCIP_OKAY;
}

/** initialization method of event handler (called after problem was transformed) */
static
SCIP_DECL_EVENTINIT(eventInitPrimalstall)
{
    SCIP_EVENTHDLRDATA* eventhdlrdata;

    assert(scip != NULL);
    assert(eventhdlr != NULL);
    assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);

    eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);
    assert(eventhdlrdata != NULL);

    // TODO: only catch event if this plugin is active
    SCIP_CALL(SCIPcatchEvent(scip, EVENT_PRIMALSTALL, eventhdlr, NULL, NULL));

    return SCIP_OKAY;
}

/** deinitialization method of event handler (called before transformed problem is freed) */
static
SCIP_DECL_EVENTEXIT(eventExitPrimalstall)
{
    SCIP_EVENTHDLRDATA* eventhdlrdata;

    assert(scip != NULL);
    assert(eventhdlr != NULL);
    assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);

    eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);
    assert(eventhdlrdata != NULL);

    SCIP_CALL(SCIPdropEvent(scip, EVENT_PRIMALSTALL, eventhdlr, NULL, -1));

    return SCIP_OKAY;
}

/** solving process initialization method of event handler (called when branch and bound process is about to begin) */
#define eventInitsolPrimalstall NULL

/** solving process deinitialization method of event handler (called before branch and bound process data is freed) */
#define eventExitsolPrimalstall NULL

/** frees specific event data */
#define eventDeletePrimalstall NULL

/** execution method of event handler */
static
SCIP_DECL_EVENTEXEC(eventExecPrimalstall)
{
    SCIP_EVENTHDLRDATA* eventhdlrdata;

    assert(eventhdlr != NULL);
    assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);
    assert(event != NULL);
    assert(scip != NULL);
    eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);
    assert(eventhdlrdata != NULL);

    switch(SCIPeventGetType(event))
    {
        SCIP_SOL* newsol;
        SCIP_Real newsolval;
    case SCIP_EVENTTYPE_BESTSOLFOUND:
        // reset time if we found really good solution
        newsol = SCIPgetBestSol(scip);
        newsolval = SCIPgetSolOrigObj(scip, newsol);
        DBG("[ primalstall ] new sol with obj %.4f\n", newsolval);
        if(significantImprovement(eventhdlrdata, newsolval, SCIPgetObjsense(scip))) {
            eventhdlrdata->lastsolval = newsolval;
            eventhdlrdata->lastsoltime = SCIPgetTotalTime(scip);
        }
        break;
    default: // must be node event (actually, we don't care)
        // stop solving if too much time has passed
        if(timeIsUp(eventhdlrdata, SCIPgetTotalTime(scip))) {
            DBG("[ primalstall ] time is up!\n");
            SCIP_CALL(SCIPinterruptSolve(scip));
        }
    }

    return SCIP_OKAY;
}

/** creates event handler for primalstall event */
SCIP_RETCODE SCIPincludeEventHdlrPrimalstall(
    SCIP*                 scip                /**< SCIP data structure */
    )
{
#ifdef TESTING
    // run tests
    runTests();
    exit(0);
#endif

    SCIP_EVENTHDLRDATA* eventhdlrdata;
    SCIP_EVENTHDLR* eventhdlr;

    /* create primalstall event handler data */
    SCIP_CALL(SCIPallocBlockMemory(scip, &eventhdlrdata));

    // initialize data values
    eventhdlrdata->abstol      = DEFAULT_ABSTOL;
    eventhdlrdata->reltol      = DEFAULT_RELTOL;
    eventhdlrdata->mintime     = DEFAULT_MINTIME;
    eventhdlrdata->maxtime     = DEFAULT_MAXTIME;
    eventhdlrdata->fractime    = DEFAULT_FRACTIME;
    eventhdlrdata->lastsolval  = DEFAULT_LASTSOLVAL;
    eventhdlrdata->lastsoltime = DEFAULT_LASTSOLTIME;

    eventhdlr = NULL;

    /* include event handler into SCIP */
    SCIP_CALL(SCIPincludeEventhdlrBasic(scip, &eventhdlr, EVENTHDLR_NAME, EVENTHDLR_DESC,
                                        eventExecPrimalstall, eventhdlrdata));
    assert(eventhdlr != NULL);

    /* set non fundamental callbacks via setter functions */
    SCIP_CALL(SCIPsetEventhdlrCopy(scip, eventhdlr, eventCopyPrimalstall));
    SCIP_CALL(SCIPsetEventhdlrFree(scip, eventhdlr, eventFreePrimalstall));
    SCIP_CALL(SCIPsetEventhdlrInit(scip, eventhdlr, eventInitPrimalstall));
    SCIP_CALL(SCIPsetEventhdlrExit(scip, eventhdlr, eventExitPrimalstall));

    /* add primalstall event handler parameters */
    SCIP_CALL(SCIPaddRealParam(scip, "limits/" EVENTHDLR_NAME "/abstol",
                               "absolute improvement tolerance",
                               &(eventhdlrdata->abstol), FALSE,
                               DEFAULT_ABSTOL, 0.0, INFINITY, NULL, NULL));
    SCIP_CALL(SCIPaddRealParam(scip, "limits/" EVENTHDLR_NAME "/reltol",
                               "relative improvement tolerance",
                               &(eventhdlrdata->reltol), FALSE,
                               DEFAULT_RELTOL, 0.0, INFINITY, NULL, NULL));
    SCIP_CALL(SCIPaddRealParam(scip, "limits/" EVENTHDLR_NAME "/mintime",
                               "minimum improvement time (seconds)",
                               &(eventhdlrdata->mintime), FALSE,
                               DEFAULT_MINTIME, 0.0, INFINITY, NULL, NULL));
    SCIP_CALL(SCIPaddRealParam(scip, "limits/" EVENTHDLR_NAME "/maxtime",
                               "maximum improvement time (seconds)",
                               &(eventhdlrdata->maxtime), FALSE,
                               DEFAULT_MAXTIME, 0.0, INFINITY, NULL, NULL));
    SCIP_CALL(SCIPaddRealParam(scip, "limits/" EVENTHDLR_NAME "/fractime",
                               "fraction of elapsed time",
                               &(eventhdlrdata->fractime), FALSE,
                               DEFAULT_FRACTIME, 0.0, 1.0, NULL, NULL));

    return SCIP_OKAY;
}
