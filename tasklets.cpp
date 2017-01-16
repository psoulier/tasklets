#include <libos/libos.h>
#include <firmware.h>
#include <wsl/wsl.h>

#include "wf_api.h"


/* This small thread provides a way for waitless functions to
 * call blocking functions in a relatively seamless manner...
 * provided you are using the Waitless Fiber infrastructure.
 *
 * When you make a WF_BLOCKING_WAIT, the Waitless Fiber State
 * is saved and the WTH is queued to a thread serviced by
 * wf_block_service. This pulls WTHs from the SHQ and simply
 * invokes WF_RESUME on them. This means the fiber resumes
 * execution at the point of the WF_BLOCKING_WAIT but this
 * time it is running in a pthread. Execution continues in this
 * pthread until a blocking call occurs at which point execution
 * will transfer back to the waitless thread.
 */

#ifndef ISESIM
static void wf_block_service( void* arg_ptr );


u32 WF_Main_Init( u32 arg )
    {
    USE_VAR( arg );

    GBL_wf_p = GBL_wf_p;
    SHQ_Init(&GBL_wf_p->blocking_call_q);
    GS_sem_init(&GBL_wf_p->blocking_call_sem);
    GS_thread_t pthread;
    GS_thread_create(&pthread, wf_block_service, NULL, WSL_DEFAULT_STACK_SIZE,
            DEFAULT_FW_THREAD_PRIORITY, WSL_EM_REGISTER);

    return SUCCESS;
    }

static void wf_block_service( void* arg_ptr )
    {
    OS_set_extension( "-wf_service" );
    USE_VAR( arg_ptr );

    while( true )
        {
        GS_sem_wait( &GBL_wf_p->blocking_call_sem );
        while( ! SHQ_IsEmpty( &GBL_wf_p->blocking_call_q ))
            {
            WTH_t* wth = SHQ_Dequeue( &GBL_wf_p->blocking_call_q );
            WF_RESUME( wth );
            }
        }

    GS_thread_exit( 0 );
    }
#endif

#ifdef __cplusplus
void wf_whoami(WTH_t *pWTH)
    {
    GBL_wf_p->pwth = pWTH;
    }

void (*__wf_whoami)(...) = (void (*)(...))wf_whoami;
#else
void wf_whoami(WTH_t *pWTH)
    {
    GBL_wf_p->pwth = pWTH;
    }
void (*__wf_whoami)(WTH_t*) = (void (*)(WTH_t*))wf_whoami;
#endif


static inline int wf_skip_whitespace(const char *str)
{
    int wscnt = 0;
    while (*str == ' ' || *str == '\t')
        {
        str += 1;
        wscnt += 1;
        }

    return wscnt;
}

static inline const char* wf_nextfield(const char *str)
{
    while (*str != '.' && *str != '\0')
        {
        str += 1;
        }

    if (*str == '.')
        {
        str += 1;
        }

    return str;
}

static bool wf_findfield(const char *Init, const char * Field)
{
    int length = strlen(Field);

    do
        {
        Init = wf_nextfield(Init);

        Init += wf_skip_whitespace(Init);

        if ( strncmp(Field, Init, length) == 0 )
            {
            Init += length;
            Init += wf_skip_whitespace(Init);
            }
        }
    while (*Init != '=' && *Init != '\0');

    return *Init == '=';
}

void __wf_initinfo(__wf_info_t *pInfo, i8 *pETFClass, char *Name,
                   const char *DefaultFiberName,  const char *VarArgs)
{
    bool    etfclasspres = wf_findfield(VarArgs, STRINGIFY(WF_INFO_ETFCLASS)),
            namepres = wf_findfield(VarArgs, STRINGIFY(WF_INFO_NAME));

    if (etfclasspres || namepres)
        {
        if (etfclasspres)
            {
            *pETFClass = pInfo->etfclass;
            }
        }
    else
        {
        if (*VarArgs != '\0')
            {
            *pETFClass = pInfo->etfclass;
            }
        }

    if (pInfo->name == NULL)
        {
        pInfo->name = DefaultFiberName;
        }

    memset(Name, ' ', __WF_ETF_NAME_SIZE);
    strncpy(Name, pInfo->name, MIN((int)__WF_ETF_NAME_SIZE, strlen(pInfo->name)));
}

void WF_CREATE(void *pWTH, vfptr Fiber)
{
    waitlessth_t        *pwth = (waitlessth_t*)pWTH;

    pwth->pnext     = NULL;
    pwth->pprev     = NULL;
    pwth->resume    = Fiber;

    GBL_wf_p->pwth = (WTH_t*)pWTH;
    GBL_wf_p->fiber = Fiber;
    GBL_wf_p->created = true;

    // Start the waitless thread...
    __WF_SAVESTACKPOS();
    Fiber(pwth);
}



bool __WF_CATCH_EXCEPTION(const char *str)
{
    bool    catche = false;

    if ( strcmp(GBL_wf_p->exception.name, str) == 0 ||
         strcmp(str, "wf_exception_t") == 0 )
        {
        catche = true;
        }

    return catche;
}

void __WF_RAISE_INIT(const char *Name, const void *Data, int Size)
{
    if ( strcmp(Name, "wf_exception_t") != 0 )
        {
        memcpy(GBL_wf_p->exception.data, Data, Size);
        GBL_wf_p->exception.name = Name;
        }

    if (GBL_wf_p->exceptionActive)
        {
        GBL_wf_p->raised = true;
        }

    GBL_wf_p->exceptionActive = true;
}
