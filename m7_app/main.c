/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpmsg_lite.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#include "board.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app.h"
#include "fsl_pwm.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define APP_TASK_STACK_SIZE (256)
#define PWM_TASK_STACK_SIZE (1024)
#ifndef LOCAL_EPT_ADDR
#define LOCAL_EPT_ADDR (30)
#endif

/* Globals */
static char app_buf[512]; /* Each RPMSG buffer can carry less than 512 payload */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
static TaskHandle_t app_task_handle = NULL;
static TaskHandle_t pwm_task_handle = NULL;

static struct rpmsg_lite_instance *volatile my_rpmsg = NULL;

static struct rpmsg_lite_endpoint *volatile my_ept = NULL;
static volatile rpmsg_queue_handle my_queue        = NULL;

#define PWM_PERIOD_VALUE  30
#define PWM_STEP_DELAY_MS 100

/* Duty cycles (percent): 0 up to 100 and back down. The final 0 comes from wrapping to index 0 */
static const uint8_t pwm_duty_cycles[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 
                                          90, 80, 70, 60, 50, 40, 30, 20, 10};
#define PWM_DUTY_COUNT (sizeof(pwm_duty_cycles) / sizeof(pwm_duty_cycles[0]))

void app_destroy_task(void)
{
    if (app_task_handle)
    {
        vTaskDelete(app_task_handle);
        app_task_handle = NULL;
    }

    if (my_ept)
    {
        rpmsg_lite_destroy_ept(my_rpmsg, my_ept);
        my_ept = NULL;
    }

    if (my_queue)
    {
        rpmsg_queue_destroy(my_rpmsg, my_queue);
        my_queue = NULL;
    }

    if (my_rpmsg)
    {
        rpmsg_lite_deinit(my_rpmsg);
        my_rpmsg = NULL;
    }
}

void app_task(void *param)
{
    volatile uint32_t remote_addr = 0U;
    void *rx_buf;
    uint32_t len;
    int32_t result;
    void *tx_buf;
    uint32_t size;

    /* Print the initial banner */
    PRINTF("\r\nRPMSG String Echo FreeRTOS RTOS API Demo...\r\n");

    my_rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);

    rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);

    my_queue = rpmsg_queue_create(my_rpmsg);
    my_ept   = rpmsg_lite_create_ept(my_rpmsg, LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, my_queue);
    /*
     * Introduce some delay to avoid NS announce message not being captured by the master side.
     * This could happen when the remote side execution is too fast and the NS announce message is triggered
     * before the nameservice_isr_cb is registered on the master side.
     */
    SDK_DelayAtLeastUs(1000000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    (void)rpmsg_ns_announce(my_rpmsg, my_ept, RPMSG_LITE_NS_ANNOUNCE_STRING, RL_NS_CREATE);

    PRINTF("\r\nNameservice sent, ready for incoming messages...\r\n");

    for (;;)
    {
        /* Get RPMsg rx buffer with message */
        result =
            rpmsg_queue_recv_nocopy(my_rpmsg, my_queue, (uint32_t *)&remote_addr, (char **)&rx_buf, &len, RL_BLOCK);
        if (result != 0)
        {
            assert(false);
        }

        /* Copy string from RPMsg rx buffer */
        assert(len < sizeof(app_buf));
        memcpy(app_buf, rx_buf, len);
        app_buf[len] = 0; /* End string by '\0' */

        if ((len == 2) && (app_buf[0] == 0xd) && (app_buf[1] == 0xa))
            PRINTF("Get New Line From Master Side\r\n");
        else
            PRINTF("Get Message From Master Side : \"%s\" [len : %d]\r\n", app_buf, len);

        /* Get tx buffer from RPMsg */
        tx_buf = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &size, RL_BLOCK);
        assert(tx_buf);
        /* Copy string to RPMsg tx buffer */
        memcpy(tx_buf, app_buf, len);
        /* Echo back received message with nocopy send */
        result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, remote_addr, tx_buf, len);
        if (result != 0)
        {
            assert(false);
        }
        /* Release held RPMsg rx buffer */
        result = rpmsg_queue_nocopy_free(my_rpmsg, rx_buf);
        if (result != 0)
        {
            assert(false);
        }
    }
}

void pwm_task(void *param)
{

    PRINTF("\r\nRPMSG String Echo FreeRTOS RTOS API Demo...\r\n");

    pwm_config_t pwmConfig;
    uint32_t pwmDutycycle = 0U;
    /*!
     * config->enableStopMode = false;
     * config->enableDozeMode = false;
     * config->enableWaitMode = false;
     * config->enableDebugMode = false;
     * config->clockSource = kPWM_LowFrequencyClock;
     * config->prescale = 0U;
     * config->outputConfig = kPWM_SetAtRolloverAndClearAtcomparison;
     * config->fifoWater = kPWM_FIFOWaterMark_2;
     * config->sampleRepeat = kPWM_EachSampleOnce;
     * config->byteSwap = kPWM_ByteNoSwap;
     * config->halfWordSwap = kPWM_HalfWordNoSwap;
     */
    PWM_GetDefaultConfig(&pwmConfig);
    PRINTF("\r\nPWM default config: clock source %d, prescale %d\r\n", pwmConfig.clockSource, pwmConfig.prescale);

    /* Initialize PWM module */
    status_t status = PWM_Init(DEMO_PWM_BASEADDR, &pwmConfig);
    if (status != kStatus_Success)
    {
        PRINTF("PWM_Init failed: status %d\r\n", status);
        /* Park the task without blocking others (vTaskDelete is disabled in FreeRTOSConfig.h) */
        for (;;)
        {
            vTaskDelay(portMAX_DELAY);
        }
    }
    PRINTF("PWM_Init OK\r\n");

    /* Three initial samples be written to the PWM Sample Register */
    for (pwmDutycycle = 0u; pwmDutycycle < 3; pwmDutycycle++)
    {
        PWM_SetSampleValue(DEMO_PWM_BASEADDR, pwmDutycycle);
    }

    /* Check and Clear interrupt status flags */
    if (PWM_GetStatusFlags(DEMO_PWM_BASEADDR))
    {
        PWM_clearStatusFlags(DEMO_PWM_BASEADDR,
                             kPWM_FIFOEmptyFlag | kPWM_RolloverFlag | kPWM_CompareFlag | kPWM_FIFOWriteErrorFlag);
    }

    /* Write the period to the PWM Period Register */
    PWM_SetPeriodValue(DEMO_PWM_BASEADDR, PWM_PERIOD_VALUE);

    PWM_StartTimer(DEMO_PWM_BASEADDR);

    PRINTF("\r\nPWM started: period %d, %d steps, %d ms per step\r\n", PWM_PERIOD_VALUE, PWM_DUTY_COUNT,
           PWM_STEP_DELAY_MS);

    uint32_t index = 0U;

    while (1)
    {
        /* Output period is PWM_PERIOD_VALUE + 2 counts, so scale percent against that */
        uint32_t sample = ((PWM_PERIOD_VALUE + 2U) * pwm_duty_cycles[index]) / 100U;
        PWM_SetSampleValue(DEMO_PWM_BASEADDR, sample);

        // PRINTF("PWM step %d: duty %d%% (sample %d)\r\n", index, pwm_duty_cycles[index], sample);

        vTaskDelay(pdMS_TO_TICKS(PWM_STEP_DELAY_MS));

        index = (index + 1U) % PWM_DUTY_COUNT;
    }
}

void app_create_task(void)
{
    if (app_task_handle == NULL &&
        xTaskCreate(app_task, "APP_TASK", APP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &app_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create application task\r\n");
        for (;;)
            ;
    }
}

void pwm_create_task(void)
{
    if (pwm_task_handle == NULL &&
        xTaskCreate(pwm_task, "PWM_TASK", PWM_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &pwm_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create pwm task\r\n");
        for (;;)
            ;
    }
}
/*!
 * @brief Main function
 */
int main(void)
{
    /* Initialize standard SDK demo application pins */
    BOARD_InitHardware();

    app_create_task();
    pwm_create_task();

    vTaskStartScheduler();

    PRINTF("Failed to start FreeRTOS on core0.\n");
    for (;;)
        ;
}
