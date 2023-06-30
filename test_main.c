#include "dk_soft_timer.h"

static uint32_t s_TickCount = 0;

//滴答定时器中断
void SysTick_Handler(void) {
    s_TickCount++;

    //软件定时器任务，可以放在主循环或者滴答定时器中断中。最终定时器的回调是在dk_timer_task调用的地方执行的。
    // dk_timer_task();
}

uint32_t sys_get_tick_count(void) {
    return s_TickCount;
}

//定时器回调，和FreeRTOS一样
static void s_time_callback( TimerHandle_t xTimer ) {
    printf("hello world!\r\n");
}

int main() {
    //初始化滴答定时器1ms节拍
    SysTick_Config(GetSysClock() / 1000);

    //软件定时器初始化
    dk_soft_timer_init(&sys_get_tick_count);

    //初始化一个软件定时器，和FreeRTOS一样使用
    xTimerHandle timer_handle = xTimerCreate("timer_handle", 10000, pdTRUE, (void *)0, s_time_callback);
    if (timer_handle != NULL) {
        xTimerStart( timer_handle, 0 );
    }

    while (1) {
        //软件定时器任务，可以放在主循环或者滴答定时器中断中。最终定时器的回调是在dk_timer_task调用的地方执行的。
        dk_timer_task();
    }
}