/*******************************************************************************
*   文件名：        dk_soft_timer.c
*   描述：             模拟FreeRTOS的软件定时器
*   作者：             Lochy.Huang
*   创建日期：   2023.06.21
*   最后更新：   2023.06.21
*   更新内容：   无
*******************************************************************************/
#include "dk_soft_timer.h"

#define tmrNO_DELAY                    ( ( TickType_t ) 0U )
#define tmrMAX_TIME_BEFORE_OVERFLOW    ( ( TickType_t ) -1 )

/* Bit definitions used in the ucStatus member of a timer structure. */
#define tmrSTATUS_IS_ACTIVE                  ( ( uint8_t ) 0x01 )
#define tmrSTATUS_IS_STATICALLY_ALLOCATED    ( ( uint8_t ) 0x02 )
#define tmrSTATUS_IS_AUTORELOAD              ( ( uint8_t ) 0x04 )

#define taskENTER_CRITICAL()
#define taskEXIT_CRITICAL()

/* The definition of the timers themselves. */
typedef struct tmrTimerControl                  /* The old naming convention is used to prevent breaking kernel aware debuggers. */
{
    const char * pcTimerName;                   /*<< Text name.  This is not used by the kernel, it is included simply to make debugging easier. */ /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
    ListItem_t xTimerListItem;                  /*<< Standard linked list item as used by all kernel features for event management. */
    uint32_t xTimerPeriodInTicks;             /*<< How quickly and often the timer expires. */
    void * pvTimerID;                           /*<< An ID to identify the timer.  This allows the timer to be identified when the same callback is used for multiple timers. */
    TimerCallbackFunction_t pxCallbackFunction; /*<< The function that will be called when the timer expires. */
    uint8_t ucStatus;                           /*<< Holds bits to say if the timer was statically allocated or not, and if it is active or not. */
} xTIMER;

typedef xTIMER Timer_t;

static List_t xActiveTimerList1;
static List_t xActiveTimerList2;
static List_t * pxCurrentTimerList;
static List_t * pxOverflowTimerList;

/*
 * Insert the timer into either xActiveTimerList1, or xActiveTimerList2,
 * depending on if the expire time causes a timer counter overflow.
 */
    static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer,
                                                  const TickType_t xNextExpiryTime,
                                                  const TickType_t xTimeNow,
                                                  const TickType_t xCommandTime ) PRIVILEGED_FUNCTION;

/*
 * Reload the specified auto-reload timer.  If the reloading is backlogged,
 * clear the backlog, calling the callback for each additional reload.  When
 * this function returns, the next expiry time is after xTimeNow.
 */
    static void prvReloadTimer( Timer_t * const pxTimer,
                                TickType_t xExpiredTime,
                                const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/*
 * An active timer has reached its expire time.  Reload the timer if it is an
 * auto-reload timer, then call its callback.
 */
    static void prvProcessExpiredTimer( const TickType_t xNextExpireTime,
                                        const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/*
 * The tick count has overflowed.  Switch the timer lists after ensuring the
 * current timer list does not still reference some timers.
 */
    static void prvSwitchTimerLists( void ) PRIVILEGED_FUNCTION;

/*
 * Obtain the current tick count, setting *pxTimerListsWereSwitched to pdTRUE
 * if a tick count overflow occurred since prvSampleTimeNow() was last called.
 */
    static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) PRIVILEGED_FUNCTION;

/*
 * If the timer list contains any active timers then return the expire time of
 * the timer that will expire first and set *pxListWasEmpty to false.  If the
 * timer list does not contain any timers then return 0 and set *pxListWasEmpty
 * to pdTRUE.
 */
    static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) PRIVILEGED_FUNCTION;
    
    static getSysTickCount_t sys_get_TickCount;

void dk_soft_timer_init(getSysTickCount_t fun) {
    sys_get_TickCount = fun;
    vListInitialise( &xActiveTimerList1 );
    vListInitialise( &xActiveTimerList2 );
    pxCurrentTimerList = &xActiveTimerList1;
    pxOverflowTimerList = &xActiveTimerList2;
}

static void prvSwitchTimerLists( void )
{
    TickType_t xNextExpireTime;
    List_t * pxTemp;

    /* The tick count has overflowed.  The timer lists must be switched.
     * If there are any timers still referenced from the current timer list
     * then they must have expired and should be processed before the lists
     * are switched. */
    while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE ) {
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );

        /* Process the expired timer.  For auto-reload timers, be careful to
         * process only expirations that occur on the current list.  Further
         * expirations must wait until after the lists are switched. */
        prvProcessExpiredTimer( xNextExpireTime, tmrMAX_TIME_BEFORE_OVERFLOW );
    }

    pxTemp = pxCurrentTimerList;
    pxCurrentTimerList = pxOverflowTimerList;
    pxOverflowTimerList = pxTemp;
}

static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) {
    TickType_t xTimeNow;
    static TickType_t xLastTime = ( TickType_t ) 0U; /*lint !e956 Variable is only accessible to one task. */

    xTimeNow = sys_get_TickCount();

    if( xTimeNow < xLastTime ) {
        prvSwitchTimerLists();
        *pxTimerListsWereSwitched = pdTRUE;
    }
    else {
        *pxTimerListsWereSwitched = pdFALSE;
    }

    xLastTime = xTimeNow;

    return xTimeNow;
}

static void prvInitialiseNewTimer( const char * const pcTimerName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
                                   const TickType_t xTimerPeriodInTicks,
                                   const UBaseType_t uxAutoReload,
                                   void * const pvTimerID,
                                   TimerCallbackFunction_t pxCallbackFunction,
                                   Timer_t * pxNewTimer )
{
    /* 0 is not a valid value for xTimerPeriodInTicks. */
    configASSERT( ( xTimerPeriodInTicks > 0 ) );

    /* Initialise the timer structure members using the function
     * parameters. */
    pxNewTimer->pcTimerName = pcTimerName;
    pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;
    pxNewTimer->pvTimerID = pvTimerID;
    pxNewTimer->pxCallbackFunction = pxCallbackFunction;
    vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );

    if( uxAutoReload != pdFALSE ) {
        pxNewTimer->ucStatus |= tmrSTATUS_IS_AUTORELOAD;
    }
}

static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer,
                                                  const TickType_t xNextExpiryTime,
                                                  const TickType_t xTimeNow,
                                                  const TickType_t xCommandTime )
{
    BaseType_t xProcessTimerNow = pdFALSE;

    listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
    listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

    if( xNextExpiryTime <= xTimeNow ) {
        /* Has the expiry time elapsed between the command to start/reset a
         * timer was issued, and the time the command was processed? */
        if( ( ( TickType_t ) ( xTimeNow - xCommandTime ) ) >= pxTimer->xTimerPeriodInTicks ) { /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
            /* The time between a command being issued and the command being
             * processed actually exceeds the timers period.  */
            xProcessTimerNow = pdTRUE;
        }
        else {
            vListInsert( pxOverflowTimerList, &( pxTimer->xTimerListItem ) );
        }
    }
    else {
        if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) ) {
            /* If, since the command was issued, the tick count has overflowed
             * but the expiry time has not, then the timer must have already passed
             * its expiry time and should be processed immediately. */
            xProcessTimerNow = pdTRUE;
        }
        else {
            vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
        }
    }

    return xProcessTimerNow;
}

static void prvReloadTimer( Timer_t * const pxTimer,
                            TickType_t xExpiredTime,
                            const TickType_t xTimeNow )
{
    /* Insert the timer into the appropriate list for the next expiry time.
     * If the next expiry time has already passed, advance the expiry time,
     * call the callback function, and try again. */
    while( prvInsertTimerInActiveList( pxTimer, ( xExpiredTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xExpiredTime ) != pdFALSE ) {
        /* Advance the expiry time. */
        xExpiredTime += pxTimer->xTimerPeriodInTicks;

        /* Call the timer callback. */
        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
    }
}

static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) {
    TickType_t xNextExpireTime;

    /* Timers are listed in expiry time order, with the head of the list
     * referencing the task that will expire first.  Obtain the time at which
     * the timer with the nearest expiry time will expire.  If there are no
     * active timers then just set the next expire time to 0.  That will cause
     * this task to unblock when the tick count overflows, at which point the
     * timer lists will be switched and the next expiry time can be
     * re-assessed.  */
    *pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );

    if( *pxListWasEmpty == pdFALSE ) {
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
    }
    else {
        /* Ensure the task unblocks when the tick count rolls over. */
        xNextExpireTime = ( TickType_t ) 0U;
    }

    return xNextExpireTime;
}

static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow ) {
    Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList ); /*lint !e9087 !e9079 void * is used as this macro is used with tasks and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same. */

    /* Remove the timer from the list of active timers.  A check has already
     * been performed to ensure the list is not empty. */
    ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );

    /* If the timer is an auto-reload timer then calculate the next
     * expiry time and re-insert the timer in the list of active timers. */
    if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0 ) {
        prvReloadTimer( pxTimer, xNextExpireTime, xTimeNow );
    }
    else {
        pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
    }

    /* Call the timer callback. */
    pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
}

/*-----------------------------------------------------------*/

TickType_t xTimerGetPeriod( TimerHandle_t xTimer )
{
    Timer_t * pxTimer = xTimer;

    configASSERT( xTimer );
    return pxTimer->xTimerPeriodInTicks;
}
/*-----------------------------------------------------------*/

void vTimerSetReloadMode( TimerHandle_t xTimer,
                          const UBaseType_t uxAutoReload )
{
    Timer_t * pxTimer = xTimer;

    configASSERT( xTimer );
    if( uxAutoReload != pdFALSE )
    {
        pxTimer->ucStatus |= tmrSTATUS_IS_AUTORELOAD;
    }
    else
    {
        pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_AUTORELOAD );
    }
}
/*-----------------------------------------------------------*/

UBaseType_t uxTimerGetReloadMode( TimerHandle_t xTimer )
{
    Timer_t * pxTimer = xTimer;
    UBaseType_t uxReturn;

    configASSERT( xTimer );
    taskENTER_CRITICAL();
    {
        if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) == 0 )
        {
            /* Not an auto-reload timer. */
            uxReturn = ( UBaseType_t ) pdFALSE;
        }
        else
        {
            /* Is an auto-reload timer. */
            uxReturn = ( UBaseType_t ) pdTRUE;
        }
    }
    taskEXIT_CRITICAL();

    return uxReturn;
}
/*-----------------------------------------------------------*/

TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
    Timer_t * pxTimer = xTimer;
    TickType_t xReturn;

    configASSERT( xTimer );
    xReturn = listGET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ) );
    return xReturn;
}
/*-----------------------------------------------------------*/

const char * pcTimerGetName( TimerHandle_t xTimer ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    Timer_t * pxTimer = xTimer;

    configASSERT( xTimer );
    return pxTimer->pcTimerName;
}

TimerHandle_t xTimerCreate( const char * const pcTimerName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
                                    const TickType_t xTimerPeriodInTicks,
                                    const UBaseType_t uxAutoReload,
                                    void * const pvTimerID,
                                    TimerCallbackFunction_t pxCallbackFunction )
{
    Timer_t * pxNewTimer;

    pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) ); /*lint !e9087 !e9079 All values returned by pvPortMalloc() have at least the alignment required by the MCU's stack, and the first member of Timer_t is always a pointer to the timer's mame. */

    if( pxNewTimer != NULL ) {
        /* Status is thus far zero as the timer is not created statically
         * and has not been started.  The auto-reload bit may get set in
         * prvInitialiseNewTimer. */
        pxNewTimer->ucStatus = 0x00;
        prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );
    }

    return pxNewTimer;
}

/*-----------------------------------------------------------*/

BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer ) {
    BaseType_t xReturn;
    Timer_t * pxTimer = xTimer;

    configASSERT( xTimer );

    /* Is the timer in the list of active timers? */
    if( ( pxTimer->ucStatus & tmrSTATUS_IS_ACTIVE ) == 0 ) {
        xReturn = pdFALSE;
    }
    else {
        xReturn = pdTRUE;
    }

    return xReturn;
} /*lint !e818 Can't be pointer to const due to the typedef. */
/*-----------------------------------------------------------*/

void * pvTimerGetTimerID( const TimerHandle_t xTimer ) {
    Timer_t * const pxTimer = xTimer;
    void * pvReturn;

    configASSERT( xTimer );

    pvReturn = pxTimer->pvTimerID;

    return pvReturn;
}
/*-----------------------------------------------------------*/

void vTimerSetTimerID( TimerHandle_t xTimer, void * pvNewID ) {
    Timer_t * const pxTimer = xTimer;

    configASSERT( xTimer );
    pxTimer->pvTimerID = pvNewID;
}

BaseType_t xTimerStart( TimerHandle_t xTimer, const TickType_t xTicksToWait ) {
    Timer_t * pxTimer = xTimer;
    BaseType_t xTimerListsWereSwitched;
    uint32_t xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

    pxTimer->ucStatus |= tmrSTATUS_IS_ACTIVE;

    if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE ) {
        /* The timer is in a list, remove it. */
        ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
    }

    if( prvInsertTimerInActiveList( pxTimer, xTimeNow + pxTimer->xTimerPeriodInTicks, xTimeNow, xTimeNow ) != pdFALSE ) {
        /* The timer expired before it was added to the active
         * timer list.  Process it now. */
        if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0 ) {
            prvReloadTimer( pxTimer, xTimeNow + pxTimer->xTimerPeriodInTicks, xTimeNow );
        }
        else {
            pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
        }

        /* Call the timer callback. */
        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
    }

    return pdTRUE;
}

BaseType_t xTimerStop( TimerHandle_t xTimer, const TickType_t xTicksToWait ) {
    xTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
    return pdTRUE;
}

BaseType_t xTimerChangePeriod( TimerHandle_t xTimer, TickType_t xNewPeriod, TickType_t xTicksToWait ) {
    Timer_t * pxTimer = xTimer;
    BaseType_t xTimerListsWereSwitched;
    uint32_t xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

    pxTimer->ucStatus |= tmrSTATUS_IS_ACTIVE;
    pxTimer->xTimerPeriodInTicks = xNewPeriod;
    configASSERT( ( pxTimer->xTimerPeriodInTicks > 0 ) );

    /* The new period does not really have a reference, and can
     * be longer or shorter than the old one.  The command time is
     * therefore set to the current time, and as the period cannot
     * be zero the next expiry time can only be in the future,
     * meaning (unlike for the xTimerStart() case above) there is
     * no fail case that needs to be handled here. */
    ( void ) prvInsertTimerInActiveList( pxTimer, ( xTimeNow + pxTimer->xTimerPeriodInTicks ), xTimeNow, xTimeNow );

    return pdTRUE;
}

BaseType_t xTimerDelete( TimerHandle_t xTimer, const TickType_t xTicksToWait ) {
    Timer_t * pxTimer = xTimer;
    if( ( pxTimer->ucStatus & tmrSTATUS_IS_STATICALLY_ALLOCATED ) == ( uint8_t ) 0 ) {
        vPortFree( pxTimer );
    }
    else {
        pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
    }

    return pdTRUE;
}

void dk_timer_task(void) {
    TickType_t xNextExpireTime;
    BaseType_t xListWasEmpty;
    BaseType_t xTimerListsWereSwitched;
    uint32_t xTimeNow;

    xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );
    xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

    if ( (xTimerListsWereSwitched == pdFALSE) && (xListWasEmpty == pdFALSE) && (xNextExpireTime <= xTimeNow) ) {
        prvProcessExpiredTimer( xNextExpireTime, xTimeNow );
    }
}





