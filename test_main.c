#include "dk_soft_timer.h"

static uint32_t s_TickCount = 0;

//�δ�ʱ���ж�
void SysTick_Handler(void) {
    s_TickCount++;

    //�����ʱ�����񣬿��Է�����ѭ�����ߵδ�ʱ���ж��С����ն�ʱ���Ļص�����dk_timer_task���õĵط�ִ�еġ�
    // dk_timer_task();
}

uint32_t sys_get_tick_count(void) {
    return s_TickCount;
}

//��ʱ���ص�����FreeRTOSһ��
static void s_time_callback( TimerHandle_t xTimer ) {
    printf("hello world!\r\n");
}

int main() {
    //��ʼ���δ�ʱ��1ms����
    SysTick_Config(GetSysClock() / 1000);

    //�����ʱ����ʼ��
    dk_soft_timer_init(&sys_get_tick_count);

    //��ʼ��һ�������ʱ������FreeRTOSһ��ʹ��
    xTimerHandle timer_handle = xTimerCreate("timer_handle", 10000, pdTRUE, (void *)0, s_time_callback);
    if (timer_handle != NULL) {
        xTimerStart( timer_handle, 0 );
    }

    while (1) {
        //�����ʱ�����񣬿��Է�����ѭ�����ߵδ�ʱ���ж��С����ն�ʱ���Ļص�����dk_timer_task���õĵط�ִ�еġ�
        dk_timer_task();
    }
}