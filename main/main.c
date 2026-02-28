// Include primary libraries
#include <stdio.h>
#include <string.h>
#include <esp_err.h>

// Include RTOS libraries
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Include Rainmaker libraries
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>

// Include systems libraries
#include "esp_log.h"
#include "sht3x.h"

// Declare queue for communication between tasks
QueueHandle_t task_queue;

// IoT task function
void iot_task(void * pvParameters)
{
    int c;
    while(1)
    {
        // Check if the queue is not empty
        if(task_queue != 0)
        {
            // Dequeue recent message
            if( xQueueReceive(task_queue, &c, (TickType_t) 10))
            {
                ESP_LOGI("main", "%d", c);
            }
        }   
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

// SHT31 task function
void sht31_task(void * pvParameters)
{
    int c = 5;
    while(1)
    {
        xQueueSend(task_queue, (void *) &c, (TickType_t) 5);
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}

// Main function
void app_main(void)
{
    // Begin Queue
    task_queue = xQueueCreate(5, sizeof(int));

    // Run the 2 tasks on separate CPUs
    xTaskCreatePinnedToCore(iot_task, "IoT Task", 8192, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(sht31_task, "SHT31 Sensor Task", 8192, NULL, 2, NULL, 1);
}
