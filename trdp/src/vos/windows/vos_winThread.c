/**********************************************************************************************************************/
/**
 * @file            windows/vos_winThread.c
 *
 * @brief           Multitasking functions using Windows thread-handling
 *
 * @details         OS abstraction of thread-handling functions
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Bernd Loehr, NewTec GmbH
 *
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright NewTec GmbH, 2018. All rights reserved.
 *
 * $Id$
 *
 *      BL 2018-08-06: CloseHandle succeeds with return value != 0
 *      SB 2018-07-25: vos_mutexLocalCreate mem allocation fixed
 *      BL 2018-06-25: Ticket #202: vos_mutexTrylock return value
 *      BL 2018-06-20: Ticket #184: Building with VS 2015: WIN64 and Windows threads (SOCKET instead of INT32)
 */

#ifndef _WIN64
#error \
    "You are trying to compile the WIN64 implementation of vos_winThread.c - either define _WIN64 or exclude this file!"
#endif

/***********************************************************************************************************************
 * INCLUDES
 */
#include <errno.h>
#include <sys/timeb.h>
#include <time.h>
#include <string.h>

#include "vos_thread.h"
#include "vos_sock.h"
#include "vos_mem.h"
#include "vos_utils.h"
#include "vos_private.h"

/***********************************************************************************************************************
 * DEFINITIONS
 */

const size_t    cDefaultStackSize   = 16 * 1024;
const UINT32    cMutextMagic        = 0x1234FEDC;

#define NSECS_PER_USEC  1000u
#define USECS_PER_MSEC  1000u
#define MSECS_PER_SEC   1000u

/* This define holds the max amount of seconds to get stored in 32bit holding micro seconds        */
/* It is the result when using the common time struct with tv_sec and tv_usec as on a 32 bit value */
/* so far 0..999999 gets used for the tv_usec field as per definition, then 0xFFF0BDC0 usec        */
/* are remaining to represent the seconds, which in turn give 0x10C5 seconds or in decimal 4293    */
#define MAXSEC_FOR_USECPRESENTATION  4293

/***********************************************************************************************************************
 *  LOCALS
 */

static BOOL8 vosThreadInitialised = FALSE;

/***********************************************************************************************************************
 * GLOBAL FUNCTIONS
 */

/**********************************************************************************************************************/
/* Threads
                                                                                                           */
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/** Cyclic thread functions.
 *  Wrapper for cyclic threads. The thread function will be called cyclically with interval.
 *
 *  @param[in]      interval        Interval for cyclic threads in us (incl. runtime)
 *  @param[in]      pFunction       Pointer to the thread function
 *  @param[in]      pArguments      Pointer to the thread function parameters
 *  @retval         void
 */


void vos_cyclicThread (
    UINT32              interval,
    VOS_THREAD_FUNC_T   pFunction,
    void                *pArguments)
{
    VOS_TIMEVAL_T   priorCall;
    VOS_TIMEVAL_T   afterCall;
    UINT32          execTime;
    UINT32          waitingTime;
    for (;; )
    {
        vos_getTime(&priorCall);  /* get initial time */
        pFunction(pArguments);    /* perform thread function */
        vos_getTime(&afterCall);  /* get time after function ghas returned */
        /* subtract in the pattern after - prior to get the runtime of function() */
        vos_subTime(&afterCall, &priorCall);
        /* afterCall holds now the time difference within a structure not compatible with interval */
        /* check if UINT32 fits to hold the waiting time value */
        if (afterCall.tv_sec <= MAXSEC_FOR_USECPRESENTATION)
        {
            /*           sec to usec conversion                               value normalized from 0 .. 999999*/
            execTime = ((afterCall.tv_sec * MSECS_PER_SEC * USECS_PER_MSEC) + (UINT32)afterCall.tv_usec);
            if (execTime > interval)
            {
                /*severe error: cyclic task time violated*/
                waitingTime = 0U;
                /* Log the runtime violation */
                vos_printLog(VOS_LOG_ERROR,
                             "cyclic thread with interval %d usec was running  %d usec\n",
                             interval, execTime);
            }
            else
            {
                waitingTime = interval - execTime;
            }
        }
        else
        {
            /* seems a very critical overflow has happened - or simply a misconfiguration */
            /* as a rough first guess use zero waiting time here */
            waitingTime = 0U;
            /* Have this value range violation logged */
            vos_printLog(VOS_LOG_ERROR,
                         "cyclic thread with interval %d usec exceeded time out by running %d sec\n",
                         interval, afterCall.tv_sec);
        }
        (void) vos_threadDelay(waitingTime);
    }
}

/**********************************************************************************************************************/
/** Initialize the thread library.
 *  Must be called once before any other call
 *
 *  @retval         VOS_NO_ERR             no error
 *  @retval         VOS_INIT_ERR           threading not supported
 */

EXT_DECL VOS_ERR_T vos_threadInit (
    void)
{
    vosThreadInitialised = TRUE;

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** De-Initialize the thread library.
 *  Must be called after last thread/timer call
 *
 */

EXT_DECL void vos_threadTerm (void)
{
    vosThreadInitialised = FALSE;
}

/**********************************************************************************************************************/
/** Create a thread.
 *  Create a thread and return a thread handle for further requests. Not each parameter may be supported by all
 *  target systems!
 *
 *  @param[out]     pThread         Pointer to returned thread handle
 *  @param[in]      pName           Pointer to name of the thread (optional)
 *  @param[in]      policy          Scheduling policy (FIFO, Round Robin or other)
 *  @param[in]      priority        Scheduling priority (1...255 (highest), default 0)
 *  @param[in]      interval        Interval for cyclic threads in us (optional)
 *  @param[in]      stackSize       Minimum stacksize, default 0: 16kB
 *  @param[in]      pFunction       Pointer to the thread function
 *  @param[in]      pArguments      Pointer to the thread function parameters
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_INIT_ERR    module not initialised
 *  @retval         VOS_NOINIT_ERR  invalid handle
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 *  @retval         VOS_THREAD_ERR  thread creation error
 *  @retval         VOS_INIT_ERR    no threads available
 */

EXT_DECL VOS_ERR_T vos_threadCreate (
    VOS_THREAD_T            *pThread,
    const CHAR8             *pName,
    VOS_THREAD_POLICY_T     policy,
    VOS_THREAD_PRIORITY_T   priority,
    UINT32                  interval,
    UINT32                  stackSize,
    VOS_THREAD_FUNC_T       pFunction,
    void                    *pArguments)
{
    HANDLE  hThread = NULL;
    DWORD   threadId;

    if (!vosThreadInitialised)
    {
        return VOS_INIT_ERR;
    }

    if ((pThread == NULL) || (pName == NULL))
    {
        return VOS_PARAM_ERR;
    }

    *pThread = NULL;


    if (interval > 0)
    {
        vos_printLog(VOS_LOG_ERROR, "%s cyclic threads not implemented yet\n", pName);
        return VOS_INIT_ERR;
    }

    /* Create the thread to begin execution on its own. */

    hThread = CreateThread(
            NULL,                                           /* default security attributes */
            (stackSize == 0) ? cDefaultStackSize : stackSize, /* use default stack size */
            (LPTHREAD_START_ROUTINE) pFunction,             /* thread function name */
            (LPVOID) pArguments,                            /* argument to thread function */
            0,                                              /* use default creation flags */
            &threadId);                                     /* returns the thread identifier */

    /* Failed? */
    if (hThread == NULL)
    {
        vos_printLog(VOS_LOG_ERROR, "%s CreateThread() failed\n", pName);
        return VOS_THREAD_ERR;
    }

    /* Set the policy of the thread? */
    if (policy != VOS_THREAD_POLICY_OTHER)
    {
        vos_printLog(VOS_LOG_WARNING,
                     "%s Thread policy other than 'default' is not supported!\n",
                     pName);
    }

    /* Set the scheduling priority of the thread */
    if ((priority > 0u) && (priority <= 255u))
    {
        /* map 1...255 to THREAD_PRIORITY_IDLE...THREAD_PRIORITY_TIME_CRITICAL*/
        const int prioMap[] = { THREAD_PRIORITY_IDLE,
                                THREAD_PRIORITY_LOWEST,
                                THREAD_PRIORITY_BELOW_NORMAL,
                                THREAD_PRIORITY_NORMAL,
                                THREAD_PRIORITY_ABOVE_NORMAL,
                                THREAD_PRIORITY_HIGHEST,
                                THREAD_PRIORITY_TIME_CRITICAL };

        (void)SetThreadPriority(hThread, prioMap[priority / 36 - 1]);
    }

    *pThread = (VOS_THREAD_T) hThread;

    return VOS_NO_ERR;
}


/**********************************************************************************************************************/
/** Terminate a thread.
 *  This call will terminate the thread with the given threadId and release all resources. Depending on the
 *  underlying architectures, it may just block until the thread ran out.
 *
 *  @param[in]      thread          Thread handle (or NULL if current thread)
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_THREAD_ERR  cancel failed
 */

EXT_DECL VOS_ERR_T vos_threadTerminate (
    VOS_THREAD_T thread)
{
    if (!vosThreadInitialised)
    {
        return VOS_INIT_ERR;
    }

    if (TerminateThread((HANDLE)thread, 0) == 0)
    {
        vos_printLog(VOS_LOG_ERROR,
                     "TerminateThread() failed (Err: %d)\n",
                     GetLastError());
        return VOS_THREAD_ERR;
    }

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Is the thread still active?
 *  This call will return VOS_NO_ERR if the thread is still active, VOS_PARAM_ERR in case it ran out.
 *
 *  @param[in]      thread          Thread handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 */

EXT_DECL VOS_ERR_T vos_threadIsActive (
    VOS_THREAD_T thread)
{
    DWORD lpExitCode;

    if (!vosThreadInitialised)
    {
        return VOS_INIT_ERR;
    }

    /* Validate the thread id. 0 sig will not kill the thread but will check if the thread is valid.*/
    if (GetExitCodeThread((HANDLE) thread, &lpExitCode) == 0)
    {
        return VOS_PARAM_ERR;
    }
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Return thread handle of calling task
 *
 *  @param[out]     pThread         pointer to thread handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 */

EXT_DECL VOS_ERR_T vos_threadSelf (
    VOS_THREAD_T *pThread)
{
    if (pThread == NULL)
    {
        return VOS_PARAM_ERR;
    }

    *pThread = (VOS_THREAD_T) GetCurrentThread();

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/*  Timers                                                                                                            */
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/** Delay the execution of the current thread by the given delay in us.
 *
 *
 *  @param[in]      delay           Delay in us
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 */

EXT_DECL VOS_ERR_T vos_threadDelay (
    UINT32 delay)
{
    /* We cannot delay less than 1ms */
    if (delay < 1000)
    {
        vos_printLog(VOS_LOG_WARNING, "Win: thread delays < 1ms are not supported!\n");
        return VOS_PARAM_ERR;
    }

    Sleep(delay / 1000u);

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Return the current time in sec and us
 *
 *
 *  @param[out]     pTime           Pointer to time value
 */

EXT_DECL void vos_getTime (
    VOS_TIMEVAL_T *pTime)
{
    struct __timeb32 curTime;

    if (pTime == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer\n");
    }
    else
    {
#ifdef __GNUC__
    #ifdef MINGW_HAS_SECURE_API
        if (_ftime_s( &curTime ) == 0)
    #else /*MINGW_HAS_SECURE_API*/
        if (_ftime( &curTime ) == 0)
    #endif  /*MINGW_HAS_SECURE_API*/
#else /*__GNUC__*/
        if (_ftime32_s( &curTime ) == 0)
#endif /*__GNUC__*/
        {
            pTime->tv_sec   = curTime.time;
            pTime->tv_usec  = curTime.millitm * 1000;
        }
        else
        {
            pTime->tv_sec   = 0;
            pTime->tv_usec  = 0;
        }
    }
}

/**********************************************************************************************************************/
/** Get a time-stamp string.
 *  Get a time-stamp string for debugging in the form "yyyymmdd-hh:mm:ss.ms"
 *  Depending on the used OS / hardware the time might not be a real-time stamp but relative from start of system.
 *
 *  @retval         timestamp   "yyyymmdd-hh:mm:ss.ms"
 */

EXT_DECL const CHAR8 *vos_getTimeStamp (void)
{
    static char         timeString[32];
    struct __timeb32    curTime;
    struct tm           curTimeTM;

    memset(timeString, 0, sizeof(timeString));

#ifdef __GNUC__
    #ifdef MINGW_HAS_SECURE_API
    if (_ftime_s( &curTime ) == 0)
    #else /*MINGW_HAS_SECURE_API*/
    if (_ftime( &curTime ) == 0)
    #endif  /*MINGW_HAS_SECURE_API*/
#else /*__GNUC__*/
    if (_ftime32_s( &curTime ) == 0)
#endif /*__GNUC__*/
    {
        if (_localtime32_s(&curTimeTM, &curTime.time) == 0)
        {
            (void) sprintf_s(timeString,
                             sizeof(timeString),
                             "%04d%02d%02d-%02d:%02d:%02d.%03hd ",
                             1900 + curTimeTM.tm_year /* offset in years from 1900 */,
                             1 + curTimeTM.tm_mon /* january is zero */,
                             curTimeTM.tm_mday,
                             curTimeTM.tm_hour,
                             curTimeTM.tm_min,
                             curTimeTM.tm_sec,
                             curTime.millitm);
        }
    }

    return timeString;
}

/**********************************************************************************************************************/
/** Clear the time stamp
 *
 *
 *  @param[out]     pTime           Pointer to time value
 */

EXT_DECL void vos_clearTime (
    VOS_TIMEVAL_T *pTime)
{
    if (pTime == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer\n");
    }
    else
    {
        timerclear(pTime);
    }
}

/**********************************************************************************************************************/
/** Add the second to the first time stamp, return sum in first
 *
 *
 *  @param[in,out]      pTime           Pointer to time value
 *  @param[in]          pAdd            Pointer to time value
 */

EXT_DECL void vos_addTime (
    VOS_TIMEVAL_T       *pTime,
    const VOS_TIMEVAL_T *pAdd)
{
    if (pTime == NULL || pAdd == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer\n");
    }
    else
    {
        pTime->tv_sec   += pAdd->tv_sec;
        pTime->tv_usec  += pAdd->tv_usec;

        while (pTime->tv_usec >= 1000000)
        {
            pTime->tv_sec++;
            pTime->tv_usec -= 1000000;
        }
    }
}

/**********************************************************************************************************************/
/** Subtract the second from the first time stamp, return diff in first
 *
 *
 *  @param[in,out]      pTime           Pointer to time value
 *  @param[in]          pSub            Pointer to time value
 */

EXT_DECL void vos_subTime (
    VOS_TIMEVAL_T       *pTime,
    const VOS_TIMEVAL_T *pSub)
{
    if (pTime == NULL || pSub == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer\n");
    }
    else
    {
        /* handle carry over:
         * when the usec are too large at pSub, move one second from tv_sec to tv_usec */
        if (pSub->tv_usec > pTime->tv_usec)
        {
            pTime->tv_sec--;
            pTime->tv_usec += 1000000;
        }

        pTime->tv_usec  = pTime->tv_usec - pSub->tv_usec;
        pTime->tv_sec   = pTime->tv_sec - pSub->tv_sec;
    }
}

/**********************************************************************************************************************/
/** Divide the first time value by the second, return quotient in first
 *
 *
 *  @param[in,out]      pTime           Pointer to time value
 *  @param[in]          divisor         Divisor
 */

EXT_DECL void vos_divTime (
    VOS_TIMEVAL_T   *pTime,
    UINT32          divisor)
{
    if ((pTime == NULL) || (divisor == 0)) /*lint !e573 mix of signed and unsigned */
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer/parameter\n");
    }
    else
    {
        UINT32 temp;

        temp = pTime->tv_sec % divisor;  /*lint !e573 mix of signed and unsigned */
        pTime->tv_sec /= divisor;        /*lint !e573 mix of signed and unsigned */
        if (temp > 0)
        {
            pTime->tv_usec += temp * 1000000;
        }
        pTime->tv_usec /= divisor;      /*lint !e573 mix of signed and unsigned */
    }
}

/**********************************************************************************************************************/
/** Multiply the first time by the second, return product in first
 *
 *
 *  @param[in,out]      pTime           Pointer to time value
 *  @param[in]          mul             Factor
 */

EXT_DECL void vos_mulTime (
    VOS_TIMEVAL_T   *pTime,
    UINT32          mul)
{
    if (pTime == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "ERROR NULL pointer/parameter\n");
    }
    else
    {
        pTime->tv_sec   *= mul;
        pTime->tv_usec  *= mul;
        while (pTime->tv_usec >= 1000000)
        {
            pTime->tv_sec++;
            pTime->tv_usec -= 1000000;
        }
    }
}

/**********************************************************************************************************************/
/** Compare the second from the first time stamp, return diff in first
 *
 *
 *  @param[in,out]      pTime           Pointer to time value
 *  @param[in]          pCmp            Pointer to time value to compare
 *  @retval             0               pTime == pCmp
 *  @retval             -1              pTime < pCmp
 *  @retval             1               pTime > pCmp
 */

EXT_DECL INT32 vos_cmpTime (
    const VOS_TIMEVAL_T *pTime,
    const VOS_TIMEVAL_T *pCmp)
{
    if (pTime == NULL || pCmp == NULL)
    {
        return 0;
    }
    if (timercmp(pTime, pCmp, >))
    {
        return 1;
    }
    if (timercmp(pTime, pCmp, <))
    {
        return -1;
    }
    return 0;
}

/**********************************************************************************************************************/
/** Get a universal unique identifier according to RFC 4122 time based version.
 *
 *
 *  @param[out]     pUuID           Pointer to a universal unique identifier
 */

EXT_DECL void vos_getUuid (
    VOS_UUID_T pUuID)
{
    /*  Manually creating a UUID from time stamp and MAC address  */
    static UINT16   count = 1;
    VOS_TIMEVAL_T   current;
    VOS_ERR_T       ret;

    vos_getTime(&current);

    pUuID[0]    = current.tv_usec & 0xFF;
    pUuID[1]    = (UINT8)((current.tv_usec & 0xFF00) >> 8);
    pUuID[2]    = (UINT8)((current.tv_usec & 0xFF0000) >> 16);
    pUuID[3]    = (UINT8)((current.tv_usec & 0xFF000000) >> 24);
    pUuID[4]    = current.tv_sec & 0xFF;
    pUuID[5]    = (current.tv_sec & 0xFF00) >> 8;
    pUuID[6]    = (UINT8)((current.tv_sec & 0xFF0000) >> 16);
    pUuID[7]    = ((current.tv_sec & 0x0F000000) >> 24) | 0x4; /*  pseudo-random version   */

    /* we always increment these values, this definitely makes the UUID unique */
    pUuID[8]    = (UINT8) (count & 0xFF);
    pUuID[9]    = (UINT8) (count >> 8);
    count++;

    /*  Copy the mac address into the rest of the array */
    ret = vos_sockGetMAC(&pUuID[10]);
    if (ret != VOS_NO_ERR)
    {
        vos_printLog(VOS_LOG_ERROR, "vos_sockGetMAC() failed (Err:%d)\n", ret);
    }
}

/**********************************************************************************************************************/
/*    Mutex & Semaphores
                                                                                                 */
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/** Create a recursive mutex.
 *  Return a mutex handle. The mutex will be available at creation.
 *
 *  @param[out]     pMutex          Pointer to mutex handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_INIT_ERR    module not initialised
 *  @retval         VOS_PARAM_ERR   pMutex == NULL
 *  @retval         VOS_MUTEX_ERR   no mutex available
 */

EXT_DECL VOS_ERR_T vos_mutexCreate (
    VOS_MUTEX_T *pMutex)
{
    int     err = 0;
    HANDLE  hMutex;

    if (pMutex == NULL)
    {
        return VOS_PARAM_ERR;
    }

    if (*pMutex == NULL)
    {
        *pMutex = (VOS_MUTEX_T) vos_memAlloc(sizeof (struct VOS_MUTEX));
    }
    if (*pMutex == NULL)
    {
        return VOS_MEM_ERR;
    }

    hMutex = CreateMutex(NULL, FALSE, NULL);

    if (hMutex == NULL)
    {
        vos_printLog(VOS_LOG_ERROR, "Can not create Mutex(winthread err=%d)\n", err);
        return VOS_MUTEX_ERR;
    }

    (*pMutex)->mutexId  = hMutex;
    (*pMutex)->magicNo  = cMutextMagic;

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Create a recursive mutex.
 *  Fill in a mutex handle. The mutex storage must be already allocated.
 *
 *  @param[out]     pMutex          Pointer to mutex handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_INIT_ERR    module not initialised
 *  @retval         VOS_PARAM_ERR   pMutex == NULL
 *  @retval         VOS_MUTEX_ERR   no mutex available
 */

VOS_ERR_T vos_mutexLocalCreate (
    struct VOS_MUTEX *pMutex)
{
    int     err = 0;
    HANDLE  hMutex;

    if (pMutex == NULL)
    {
        return VOS_PARAM_ERR;
    }

    hMutex = CreateMutex(NULL, FALSE, NULL);

    if (hMutex == NULL)
    {
        vos_printLog(VOS_LOG_ERROR, "Can not create Mutex(winthread err=%d)\n", err);
        return VOS_MUTEX_ERR;
    }

    pMutex->mutexId  = hMutex;
    pMutex->magicNo  = cMutextMagic;

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Delete a mutex.
 *  Release the resources taken by the mutex.
 *
 *  @param[in]      pMutex          mutex handle
 */

EXT_DECL void vos_mutexDelete (
    VOS_MUTEX_T pMutex)
{
    if ((pMutex == NULL) || (pMutex->magicNo != cMutextMagic))
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_mutexDelete() ERROR invalid parameter");
    }
    else
    {
        if (CloseHandle(pMutex->mutexId) != 0)
        {
            pMutex->magicNo = 0;
        }
        else
        {
            vos_printLog(VOS_LOG_ERROR,
                         "Can not destroy Mutex (Mutex error err=%d)\n", GetLastError());
        }
    }
}

/**********************************************************************************************************************/
/** Delete a mutex.
 *  Release the resources taken by the mutex.
 *
 *  @param[in]      pMutex          Pointer to mutex struct
 */

void vos_mutexLocalDelete (
    struct VOS_MUTEX *pMutex)
{
    vos_mutexDelete(pMutex);
}

/**********************************************************************************************************************/
/** Take a mutex.
 *  Wait for the mutex to become available (lock).
 *
 *  @param[in]      pMutex          mutex handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   pMutex == NULL or wrong type
 *  @retval         VOS_MUTEX_ERR   no such mutex
 */

EXT_DECL VOS_ERR_T vos_mutexLock (
    VOS_MUTEX_T pMutex)
{
    DWORD dwWaitResult;

    if (pMutex == NULL || pMutex->magicNo != cMutextMagic)
    {
        return VOS_PARAM_ERR;
    }

    dwWaitResult = WaitForSingleObject(pMutex->mutexId,    /* handle to mutex */
                                       INFINITE);  /* no time-out interval */

    if (dwWaitResult != WAIT_OBJECT_0)
    {
        vos_printLog(VOS_LOG_ERROR,
                     "Unable to lock Mutex (winthread err=%d)\n",
                     GetLastError());
        return VOS_MUTEX_ERR;
    }

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Try to take a mutex.
 *  If mutex can't be taken VOS_MUTEX_ERR is returned.
 *
 *  @param[in]      pMutex          mutex handle
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_PARAM_ERR   pMutex == NULL or wrong type
 *  @retval         VOS_MUTEX_ERR   mutex not locked
 */

EXT_DECL VOS_ERR_T vos_mutexTryLock (
    VOS_MUTEX_T pMutex)
{
    DWORD dwWaitResult;

    if (pMutex == NULL || pMutex->magicNo != cMutextMagic)
    {
        return VOS_PARAM_ERR;
    }

    dwWaitResult = WaitForSingleObject(pMutex->mutexId,    /* handle to mutex */
                                       0u);                 /* no time-out interval */
    switch (dwWaitResult)
    {
       case WAIT_OBJECT_0:
            return VOS_NO_ERR;
        case WAIT_FAILED:
           vos_printLog(VOS_LOG_ERROR,
                        "Unable to trylock Mutex (Mutex err=%d)\n",
                        GetLastError());
           return VOS_MUTEX_ERR;
       case WAIT_TIMEOUT:
       case WAIT_ABANDONED:
           return VOS_INUSE_ERR;
       default:
           return VOS_MUTEX_ERR;
    }

    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Release a mutex.
 *  Unlock the mutex.
 *
 *  @param[in]      pMutex           mutex handle
 */

EXT_DECL VOS_ERR_T vos_mutexUnlock (
    VOS_MUTEX_T pMutex)
{
    if (pMutex == NULL || pMutex->magicNo != cMutextMagic)
    {

        vos_printLogStr(VOS_LOG_ERROR, "vos_mutexUnlock() ERROR invalid parameter");
        return VOS_PARAM_ERR;
    }
    else
    {
        if (ReleaseMutex(pMutex->mutexId) == 0)
        {
            vos_printLog(VOS_LOG_ERROR,
                         "Unable to unlock Mutex (Mutex err=%d)\n",
                         GetLastError());
            return VOS_MUTEX_ERR;
        }
    }
    return VOS_NO_ERR;
}

/**********************************************************************************************************************/
/** Create a semaphore.
 *  Return a semaphore handle. Depending on the initial state the semaphore will be available on creation or not.
 *
 *  @param[out]     pSema           Pointer to semaphore handle
 *  @param[in]      initialState    The initial state of the sempahore
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_INIT_ERR    module not initialised
 *  @retval         VOS_PARAM_ERR   parameter out of range/invalid
 *  @retval         VOS_SEMA_ERR    no semaphore available
 */

EXT_DECL VOS_ERR_T vos_semaCreate (
    VOS_SEMA_T          *pSema,
    VOS_SEMA_STATE_T    initialState)
{
    VOS_ERR_T retVal = VOS_SEMA_ERR;

    /*Check parameters*/
    if (pSema == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_SemaCreate() ERROR invalid parameter pSema == NULL\n");
        retVal = VOS_PARAM_ERR;
    }
    else if ((initialState != VOS_SEMA_EMPTY) && (initialState != VOS_SEMA_FULL))
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_SemaCreate() ERROR invalid parameter initialState\n");
        retVal = VOS_PARAM_ERR;
    }
    else
    {
        /* Parameters are OK
           Create a semaphore with initial and max counts of MAX_SEM_COUNT */

        if (*pSema == NULL)
        {
            *pSema = (VOS_SEMA_T) vos_memAlloc(sizeof (VOS_SEMA_T));
        }
        if (*pSema == NULL)
        {
            return VOS_MEM_ERR;
        }

        (*pSema)->semaphore = CreateSemaphore(
                NULL,                                 /* default security attributes */
                initialState,                         /* initial count empty = 0, full = 1 */
                MAX_SEM_COUNT,                        /* maximum count */
                NULL);                                /* unnamed semaphore */

        if ((*pSema)->semaphore == NULL)
        {
            /* Semaphore init failed */
            vos_printLogStr(VOS_LOG_ERROR, "vos_semaCreate() ERROR Semaphore could not be initialized\n");
            retVal = VOS_SEMA_ERR;
        }
        else
        {
            /* Semaphore init successful */
            retVal = VOS_NO_ERR;
        }
    }
    return retVal;
}

/**********************************************************************************************************************/
/** Delete a semaphore.
 *  This will eventually release any processes waiting for the semaphore.
 *
 *  @param[in]      sema            semaphore handle
 */

EXT_DECL void vos_semaDelete (
    VOS_SEMA_T sema)
{
    /* Check parameter */
    if (sema == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_semaDelete() ERROR invalid parameter\n");
    }
    else
    {
        /* Check if this is a valid semaphore handle*/
        CloseHandle(sema->semaphore);
    }
    return;
}

/**********************************************************************************************************************/
/** Take a semaphore.
 *  Try to get (decrease) a semaphore.
 *
 *  @param[in]      sema            semaphore handle
 *  @param[in]      timeout         Max. time in us to wait, 0 means no wait
 *  @retval         VOS_NO_ERR      no error
 *  @retval         VOS_NOINIT_ERR  invalid handle
 *  @retval         VOS_SEMA_ERR    could not get semaphore in time
 */

EXT_DECL VOS_ERR_T vos_semaTake (
    VOS_SEMA_T  sema,
    UINT32      timeout)
{
    DWORD       dwWaitResult;
    VOS_ERR_T   retVal = VOS_SEMA_ERR;

    /* Check parameter */
    if (sema == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_semaTake() ERROR invalid parameter 'sema' == NULL\n");
        retVal = VOS_NOINIT_ERR;
    }

    dwWaitResult = WaitForSingleObject(sema->semaphore,                 /* handle to semaphore */
                                       timeout / USECS_PER_MSEC);       /* no time-out interval */
    switch (dwWaitResult)
    {
       case WAIT_OBJECT_0:
           return VOS_NO_ERR;
       case WAIT_FAILED:
           vos_printLog(VOS_LOG_ERROR,
                        "Unable to trylock Mutex (Mutex err=%d)\n",
                        GetLastError());
           return VOS_SEMA_ERR;
       case WAIT_TIMEOUT:
       default:
           break;
    }

    return retVal;
}


/**********************************************************************************************************************/
/** Give a semaphore.
 *  Release (increase) a semaphore.
 *
 *  @param[in]      sema            semaphore handle
 */

EXT_DECL void vos_semaGive (
    VOS_SEMA_T sema)
{
    LONG previousCount;
    /* Check parameter */
    if (sema == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "vos_semaGive() ERROR invalid parameter 'sema' == NULL\n");
    }
    else
    {
        /* release semaphore */
        if (ReleaseSemaphore(sema->semaphore, 1, &previousCount) == 0)
        {
            /* Semaphore released */
        }
        else
        {
            /* Could not release Semaphore */
            vos_printLogStr(VOS_LOG_ERROR, "vos_semaGive() ERROR could not release semaphore\n");
        }
    }
    return;
}