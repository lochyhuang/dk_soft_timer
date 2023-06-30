/*******************************************************************************
*   文件名：        dk_soft_timer.h
*   描述：             模拟FreeRTOS的软件定时器
*   作者：             Lochy.Huang
*   创建日期：   2023.06.21
*   最后更新：   2023.06.21
*   更新内容：   无
*******************************************************************************/

#ifndef USER_DRIVER_INC_DK_SOFT_TIMER_H_
#define USER_DRIVER_INC_DK_SOFT_TIMER_H_

#include "dk_typedef.h"
#include "list.h"

struct tmrTimerControl;
typedef struct tmrTimerControl * TimerHandle_t;
#define xTimerHandle            TimerHandle_t

typedef void (* TimerCallbackFunction_t)( TimerHandle_t xTimer );
typedef uint32_t (* getSysTickCount_t)();

TickType_t xTimerGetPeriod( TimerHandle_t xTimer );
void vTimerSetReloadMode( TimerHandle_t xTimer,
                          const UBaseType_t uxAutoReload );
UBaseType_t uxTimerGetReloadMode( TimerHandle_t xTimer );
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer );
const char * pcTimerGetName( TimerHandle_t xTimer );
TimerHandle_t xTimerCreate( const char * const pcTimerName,
                                    const TickType_t xTimerPeriodInTicks,
                                    const UBaseType_t uxAutoReload,
                                    void * const pvTimerID,
                                    TimerCallbackFunction_t pxCallbackFunction );
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer );
void * pvTimerGetTimerID( const TimerHandle_t xTimer );
void vTimerSetTimerID( TimerHandle_t xTimer, void * pvNewID );
BaseType_t xTimerStart( TimerHandle_t xTimer, const TickType_t xTicksToWait );
BaseType_t xTimerStop( TimerHandle_t xTimer, const TickType_t xTicksToWait );
BaseType_t xTimerChangePeriod( TimerHandle_t xTimer, TickType_t xNewPeriod, TickType_t xTicksToWait );

void dk_soft_timer_init(getSysTickCount_t fun);
void dk_timer_task(void);

#define xTimerReset     xTimerStart

#endif /* USER_DRIVER_INC_DK_SOFT_TIMER_H_ */
