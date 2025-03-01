/*
 * CTU/EMBO - EMBedded Oscilloscope <github.com/parezj/EMBO>
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#include "cfg.h"
#include "app.h"
#include "app_data.h"
#include "app_sync.h"

#include "comm.h"
#include "comm_proto.h"
#include "periph.h"
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#ifdef EM_SYSVIEW
#include "SEGGER_SYSVIEW.h"
#endif


#define EM_PRI_T1       3  // wd
#define EM_PRI_T2       2  // trig_check        // CHANGED 10.6 from value 1
#define EM_PRI_T3       5  // trig_post_count
#define EM_PRI_T4       1  // comm_and_init     // CHANGED 10.6 from value 2
#define EM_PRI_T5       4  // cntr

void t1_wd(void* p);
void t2_trig_check(void* p);
void t3_trig_post_count(void* p);
void t4_comm_and_init(void* p);
void t5_cntr(void* p);

StackType_t stack_t1[EM_STACK_T1];
StackType_t stack_t2[EM_STACK_T2];
StackType_t stack_t3[EM_STACK_T3];
StackType_t stack_t4[EM_STACK_T4];
StackType_t stack_t5[EM_STACK_T5];

StaticTask_t buff_t1;
StaticTask_t buff_t2;
StaticTask_t buff_t3;
StaticTask_t buff_t4;
StaticTask_t buff_t5;

volatile uint8_t init_done = 0;   // system initialized

#ifdef EM_DEBUG
volatile UBaseType_t watermark_t1 = -1;
volatile UBaseType_t watermark_t2 = -1;
volatile UBaseType_t watermark_t3 = -1;
volatile UBaseType_t watermark_t4 = -1;
volatile UBaseType_t watermark_t5 = -1;
#endif

/* NOTE: without optimizations -Os (for size) vTaskDelay or context switch
 * cause probably stack overflow somewhere, because hard fault. its weird
 */

void app_main(void)
{
    __disable_irq();

    /* crucial for FreeRTOS on M3 and M4 */
#ifdef NVIC_PRIORITYGROUP_4
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
#endif

    /* SysTick */
    SysTick->VAL   = 0UL;
    LL_Init1msTick(SystemCoreClock);
    NVIC_SetPriority(SysTick_IRQn, EM_IT_PRI_SYST);
    NVIC_EnableIRQ(SysTick_IRQn);
    LL_SYSTICK_EnableIT();

    /* Semaphores */
    sem1_comm = xSemaphoreCreateBinaryStatic(&buff_sem1_comm);
    sem2_trig = xSemaphoreCreateBinaryStatic(&buff_sem2_trig);
    sem3_cntr = xSemaphoreCreateBinaryStatic(&buff_sem3_cntr);
    mtx1 = xSemaphoreCreateMutexStatic(&buff_mtx1);

    ASSERT(sem1_comm != NULL);
    ASSERT(sem2_trig != NULL);
    ASSERT(sem3_cntr != NULL);
    ASSERT(mtx1 != NULL);

    /* Tasks */
    ASSERT(xTaskCreateStatic(t1_wd, "wd", EM_STACK_T1, NULL, EM_PRI_T1, stack_t1, &buff_t1) != NULL);
    ASSERT(xTaskCreateStatic(t2_trig_check, "trig_check", EM_STACK_T2, NULL, EM_PRI_T2, stack_t2, &buff_t2) != NULL);
    ASSERT(xTaskCreateStatic(t3_trig_post_count, "trig_post_count", EM_STACK_T3, NULL, EM_PRI_T3, stack_t3, &buff_t3) != NULL);
    ASSERT(xTaskCreateStatic(t4_comm_and_init, "comm_and_init", EM_STACK_T4, NULL, EM_PRI_T4, stack_t4, &buff_t4) != NULL);
    ASSERT(xTaskCreateStatic(t5_cntr, "cntr", EM_STACK_T5, NULL, EM_PRI_T5, stack_t5, &buff_t5) != NULL);

    __enable_irq();

    init_done = true;

    vTaskStartScheduler(); // start scheduler

    ASSERT(0); // never get here
}

void t1_wd(void* p)
{
    while (!init_done)
        vTaskDelay(2);

    while(1)
    {
        iwdg_feed(); // feed watchdog
        led_blink_do(&led, daq.uwTick); // blink led optionaly

        vTaskDelay(10);

#ifdef EM_DEBUG
        watermark_t1 = uxTaskGetStackHighWaterMark(NULL);
#endif
    }
}

void t2_trig_check(void* p)
{
    while (!init_done)
        vTaskDelay(2);

    while(1)
    {
        ASSERT(xSemaphoreTake(mtx1, portMAX_DELAY) == pdPASS);

        daq_trig_check(&daq); // check pre-trigger, auto-trigger, send Ready

        ASSERT(xSemaphoreGive(mtx1) == pdPASS);

        vTaskDelay(5);

#ifdef EM_DEBUG
        watermark_t2 = uxTaskGetStackHighWaterMark(NULL);
#endif
    }
}

void t3_trig_post_count(void* p)
{
    while (!init_done)
        vTaskDelay(2);

    while(1)
    {
        ASSERT(xSemaphoreTake(sem2_trig, portMAX_DELAY) == pdPASS);
        ASSERT(xSemaphoreTake(mtx1, portMAX_DELAY) == pdPASS);

        daq_trig_postcount(&daq); // count post-trigger and send Ready

        ASSERT(xSemaphoreGive(mtx1) == pdPASS);

#ifdef EM_DEBUG
        watermark_t3 = uxTaskGetStackHighWaterMark(NULL);
#endif
    }
}

void t4_comm_and_init(void* p)
{
    /* init modules */
    pwm_init(&pwm);
    led_init(&led);
    cntr_init(&cntr);
    comm_init(&comm);
    daq_init(&daq);
    daq_mode_set(&daq, VM);
    led_blink_set(&led, 3, EM_BLINK_LONG_MS, daq.uwTick);

#ifdef EM_DAC
    sgen_init(&sgen);
#endif

#ifdef EM_SYSVIEW
    SEGGER_SYSVIEW_Conf();
#endif

    while (EM_VM_ReadQ(NULL) == SCPI_RES_ERR) // read vcc
    {
        for (int i = 0; i < 10000; i++) // wait data ready
            __asm("nop");
    }

    //iwdg_feed();
    init_done = 1;

#ifdef EM_DEBUG
        watermark_t4 = uxTaskGetStackHighWaterMark(NULL);
#endif

    while(1)
    {
        ASSERT(xSemaphoreTake(sem1_comm, portMAX_DELAY) == pdPASS);
        ASSERT(xSemaphoreTake(mtx1, portMAX_DELAY) == pdPASS);

        if (comm_main(&comm) == EM_TRUE) // check if new message is in buffer
            led_blink_set(&led, 1, EM_BLINK_SHORT_MS, daq.uwTick); // toggle green led

        comm.uart.available = EM_FALSE;
        comm.usb.available = EM_FALSE;

        ASSERT(xSemaphoreGive(mtx1) == pdPASS);

#ifdef EM_DEBUG
        watermark_t4 = uxTaskGetStackHighWaterMark(NULL);
#endif
    }
}

void t5_cntr(void* p)
{
    while (!init_done)
        vTaskDelay(2);

    while(1)
    {
        ASSERT(xSemaphoreTake(sem3_cntr, portMAX_DELAY) == pdPASS);

        while (cntr.enabled)
        {
            cntr_meas(&cntr); // calc freq from counter measured data
            vTaskDelay(50);

#ifdef EM_DEBUG
        watermark_t5 = uxTaskGetStackHighWaterMark(NULL);
#endif
        }
    }
}


