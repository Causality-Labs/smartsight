/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rpmsg_lite.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#include "board.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "app.h"
#include "fsl_pwm.h"
#include "pan_tilt_mount.h"
#include "emulate_path.h"
#include "smartsight_message.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define MESSAGE_TASK_STACK_SIZE (256)
#define PAN_TILT_TASK_STACK_SIZE (1024)
#ifndef LOCAL_EPT_ADDR
#define LOCAL_EPT_ADDR (30)
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
static TaskHandle_t message_task_handle = NULL;
static TaskHandle_t pan_tilt_task_handle = NULL;

static struct rpmsg_lite_instance *volatile my_rpmsg = NULL;

static struct rpmsg_lite_endpoint *volatile my_ept = NULL;
static volatile rpmsg_queue_handle my_queue        = NULL;

#define PAN_TILT_STEP_DELAY_MS 20U
#define MESSAGE_TASK_DELAY_MS 100U

static QueueHandle_t xPositionQueue;
static QueueHandle_t xCtrlQueue;

typedef enum
{
    ENABLED_STATE = 1,
    DISABLED_STATE
} mount_state_t;

void message_destroy_task(void)
{
    if (message_task_handle)
    {
        vTaskDelete(message_task_handle);
        message_task_handle = NULL;
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

void message_task(void *param)
{
    volatile uint32_t remote_addr = 0U;
    ss_cmd_t ss_cmd;
    uint32_t len;
    int32_t result;

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
    (void)rpmsg_ns_announce(my_rpmsg, my_ept, "rpmsg-raw", RL_NS_CREATE);

    PRINTF("\r\nNameservice sent, ready for incoming messages...\r\n");

    for (;;)
    {
        /* Get RPMsg rx buffer with message */
        result = rpmsg_queue_recv(my_rpmsg,
                                  my_queue,
                                  (uint32_t *)&remote_addr, 
                                  (char *)&ss_cmd,
                                  sizeof(ss_cmd),
                                  &len,
                                  RL_BLOCK);
        if (result != 0)
        {
            continue;
        }

        switch (ss_cmd.header.opcode)
        {
            case SS_OP_MOVE:
            {
                pan_tilt_position_t position_data;
                position_data.pan_position  = ss_cmd.payload.position.pan_cdeg / 100.0f;
                position_data.tilt_position  = ss_cmd.payload.position.tilt_cdeg / 100.0f;
                xQueueOverwrite(xPositionQueue, &position_data);
                break;
            }
            case SS_OP_CTRL:
            {
                if (ss_cmd.payload.ctrl.ctrl < SS_CTRL_ENABLE
                    || ss_cmd.payload.ctrl.ctrl > SS_CTRL_HOME)
                {
                    PRINTF("\r\nInvalid CTRL payload 0x%02X\r\n", ss_cmd.payload.ctrl.ctrl);
                    break;
                }

                if (xQueueSend(xCtrlQueue, &ss_cmd.payload.ctrl.ctrl, 0) != pdPASS)
                {
                    PRINTF("\r\nCtrl Queue is full message was dropped...\r\n");
                }
                break;
            }
            default:
                break;

        }

        // /* Get tx buffer from RPMsg */
        // tx_buf = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &size, RL_BLOCK);
        // assert(tx_buf);
        // /* Copy string to RPMsg tx buffer */
        // memcpy(tx_buf, app_buf, len);
        // /* Echo back received message with nocopy send */
        // result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, remote_addr, tx_buf, len);
        // if (result != 0)
        // {
        //     assert(false);
        // }
        // /* Release held RPMsg rx buffer */
        // result = rpmsg_queue_nocopy_free(my_rpmsg, rx_buf);
        // if (result != 0)
        // {
        //     assert(false);
        // }
    }
}

void pan_tilt_task(void *param)
{
    PRINTF("\r\nPan Tilt Task starting..\r\n");

    static const pan_tilt_mount_config_t pan_tilt_mount_config = {
        .pan = {
            .channel       = PWM_CHANNEL_4,
            .pulse_min_us  = 500U,
            .pulse_max_us  = 2400U,
            .min_angle_deg = 0.0f,
            .max_angle_deg = 180.0f,
            .offset_deg    = 0.0f,
        },
        .tilt = {
            .channel       = PWM_CHANNEL_1,
            .pulse_min_us  = 500U,
            .pulse_max_us  = 2400U,
            .min_angle_deg = 150.0f,
            .max_angle_deg = 180.0f,
            .offset_deg    = 0.0f,
        },
        .pan_home_deg  = 90.0f,
        .tilt_home_deg = 165.0f,
        .max_pan_step_deg  = 1.0f,
        .max_tilt_step_deg  = 1.0f,
    };

    pan_tilt_mount_t mount = {0};
    pan_tilt_position_t position = {0};
    uint8_t ctrl_cmd = 0;
    mount_state_t mount_state = DISABLED_STATE;

    if (pan_tilt_mount_init(&mount, &pan_tilt_mount_config) != PAN_TILT_MOUNT_STATUS_OK)
    {
        PRINTF("Failed to initialize Pan Tilt Mount\r\n");
        assert(false);
    }

    vTaskDelay(pdMS_TO_TICKS(2000)); // Wait for Servo to Home.

    if (pan_tilt_mount_disable(&mount) != PAN_TILT_MOUNT_STATUS_OK)
    {
        PRINTF("Failed to disable Pan Tilt Mount..\r\n");
        assert(false);
    }

    for (;;)
    {
        switch (mount_state)
        {
            case ENABLED_STATE:
            {
                if (xQueueReceive(xCtrlQueue, &ctrl_cmd, 0) == pdPASS)
                {
                    if (ctrl_cmd == SS_CTRL_DISABLE)
                    {
                        pan_tilt_mount_stop(&mount);
                        pan_tilt_mount_disable(&mount);
                        mount_state = DISABLED_STATE;
                    }
                    else if (ctrl_cmd == SS_CTRL_HOME)
                    {
                        pan_tilt_mount_home(&mount);
                    }
                }

                else if (xQueueReceive(xPositionQueue, &position, 0) == pdPASS)
                {
                    pan_tilt_mount_target_position(&mount,
                                                   position.pan_position,
                                                    position.tilt_position);
                }

                if (mount_state == ENABLED_STATE)
                    pan_tilt_mount_move(&mount);

                vTaskDelay(pdMS_TO_TICKS(PAN_TILT_STEP_DELAY_MS));
                break;
            }

            case DISABLED_STATE:
            {
                if (xQueueReceive(xCtrlQueue, &ctrl_cmd, 0) == pdPASS)
                {
                    if (ctrl_cmd == SS_CTRL_ENABLE)
                    {
                        pan_tilt_mount_enable(&mount);
                        mount_state = ENABLED_STATE;
                    }
                    break;
                }

                if (xQueueReceive(xPositionQueue, &position, 0) == pdPASS)
                {
                    PRINTF("Rejected positonal command while disabled\r\n");
                }

                vTaskDelay(pdMS_TO_TICKS(PAN_TILT_STEP_DELAY_MS));
                break;
            }

            default: /* Impossible to reach this state. */
            {
                vTaskDelay(pdMS_TO_TICKS(PAN_TILT_STEP_DELAY_MS));
                break;
            }
        }
    }
}


void message_create_task(void)
{
    if (message_task_handle == NULL &&
        xTaskCreate(message_task, "MESSAGE_TASK", MESSAGE_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &message_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create message task\r\n");
        for (;;)
            ;
    }
}

void pan_tilt_create_task(void)
{
    if (pan_tilt_task_handle == NULL &&
        xTaskCreate(pan_tilt_task, "PAN_TILT_TASK", PAN_TILT_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1,
                    &pan_tilt_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create pan tilt task\r\n");
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

    xPositionQueue = xQueueCreate(1, sizeof(pan_tilt_position_t));
    if (xPositionQueue == NULL)
    {
        assert(false);
    }

    xCtrlQueue = xQueueCreate(3, sizeof(uint8_t));
    if (xCtrlQueue == NULL)
    {
        assert(false);
    }

    message_create_task();
    pan_tilt_create_task();

    vTaskStartScheduler();

    PRINTF("Failed to start FreeRTOS on core0.\n");
    for (;;)
        ;
}
