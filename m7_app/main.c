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
#include "servo.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define APP_TASK_STACK_SIZE (256)
#define PAN_TASK_STACK_SIZE (1024)
#define TILT_TASK_STACK_SIZE (1024)
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
static TaskHandle_t pan_task_handle = NULL;
static TaskHandle_t tilt_task_handle = NULL;

static struct rpmsg_lite_instance *volatile my_rpmsg = NULL;

static struct rpmsg_lite_endpoint *volatile my_ept = NULL;
static volatile rpmsg_queue_handle my_queue        = NULL;

/* Pan sweep: PAN_STEP_DEG every PAN_STEP_DELAY_MS between 0 and PAN_ANGLE_MAX_DEG.
 * Don't go below 20 ms: that's one servo frame, faster updates overflow the PWM FIFO */
#define PAN_ANGLE_MAX_DEG  180.0f
#define PAN_STEP_DEG       1.0f
#define PAN_STEP_DELAY_MS  20U

/* Tilt sweep: same as pan, on its own servo */
#define TILT_ANGLE_MIN_DEG  150.0f
#define TILT_ANGLE_MAX_DEG  180.0f
#define TILT_STEP_DEG       1.0f
#define TILT_STEP_DELAY_MS  20U

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

void pan_task(void *param)
{

    PRINTF("\r\nPan Servo Task starting..\r\n");
    servo_t pan_servo = {0};
    float pan_angle = 0.0f;
    float pan_step  = PAN_STEP_DEG;
    if (servo_init(&pan_servo, PWM_CHANNEL_4, pan_angle) != SERVO_STATUS_OK)
    {
        PRINTF("\r\nFailed to initialize Pan Servo..\r\n");
        assert(false);
    }


    while (1)
    {
        servo_set_position(&pan_servo, pan_angle);

        pan_angle += pan_step;
        if (pan_angle >= PAN_ANGLE_MAX_DEG || pan_angle <= 0.0f)
        {
            pan_step = -pan_step; /* Reverse at either end */
        }

        vTaskDelay(pdMS_TO_TICKS(PAN_STEP_DELAY_MS));
    }
}

void tilt_task(void *param)
{

    PRINTF("\r\nTilt Servo Task starting..\r\n");
    servo_t tilt_servo = {0};
    float tilt_angle = 180;
    float tilt_step  = -TILT_STEP_DEG; /* Start at 180, head down to 150 */
    if (servo_init(&tilt_servo, PWM_CHANNEL_1, tilt_angle) != SERVO_STATUS_OK)
    {
        PRINTF("\r\nFailed to initialize Tilt Servo..\r\n");
        assert(false);
    }


    while (1)
    {
        servo_set_position(&tilt_servo, tilt_angle);

        tilt_angle += tilt_step;
        if (tilt_angle >= TILT_ANGLE_MAX_DEG || tilt_angle <= TILT_ANGLE_MIN_DEG)
        {
            tilt_step = -tilt_step; /* Reverse at either end */
        }

        vTaskDelay(pdMS_TO_TICKS(TILT_STEP_DELAY_MS));
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

void pan_create_task(void)
{
    if (pan_task_handle == NULL &&
        xTaskCreate(pan_task, "PAN_TASK", PAN_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &pan_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create pan task\r\n");
        for (;;)
            ;
    }
}

void tilt_create_task(void)
{
    if (tilt_task_handle == NULL &&
        xTaskCreate(tilt_task, "TILT_TASK", TILT_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &tilt_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create tilt task\r\n");
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
    pan_create_task();
    tilt_create_task();

    vTaskStartScheduler();

    PRINTF("Failed to start FreeRTOS on core0.\n");
    for (;;)
        ;
}
