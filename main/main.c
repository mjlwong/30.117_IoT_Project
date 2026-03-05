// Include primary libraries
#include <stdio.h>
#include <string.h>
#include <esp_err.h>

// Include RTOS libraries
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

// Include Rainmaker libraries
#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include "app_network.h"
#include "app_insights.h"

// Include systems libraries
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "sht3x.h"

// Declare queue for communication between tasks
QueueHandle_t task_queue;

// Tag for log message
static const char * TAG = "main";

// Declare SHT31 sensor device object for interfacing
#if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_SENSOR_ONLY
static sht3x_t sht31_dev;
#endif

// Declare SHT31 sensor device object for Rainmaker
#if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_RAINMAKER_ONLY 
esp_rmaker_device_t * sht31_device;
#endif

// SHT31 task function
void sht31_task(void * pvParameters)
{
    // Run code if configured to use SHT31 sensor
    #if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_SENSOR_ONLY

    // Declare floats for temperature and humidity
    float temperature_humidity[2];

    // Begin I2C
    ESP_ERROR_CHECK(i2cdev_init());

    // Set the memory allocation of the SHT31 device object
    memset(&sht31_dev, 0, sizeof(sht3x_t));

    // Initialise the SHT31 device object
    ESP_ERROR_CHECK(sht3x_init_desc(&sht31_dev, 
                                    CONFIG_SHT3X_ADDR, 
                                    0, 
                                    CONFIG_I2C_MASTER_SDA, 
                                    CONFIG_I2C_MASTER_SCL));
    ESP_ERROR_CHECK(sht3x_init(&sht31_dev));

    while(1)
    {
        // Measure the temperature and humidity value from the SHT31 sensor
        ESP_ERROR_CHECK(sht3x_measure(&sht31_dev, &temperature_humidity[0], &temperature_humidity[1]));
        // Send humidity data to queue
        xQueueSend(task_queue, (void *) &temperature_humidity, (TickType_t) 5);
        // Delay f0r 50ms
        vTaskDelay(500/portTICK_PERIOD_MS);
    }

    #else // Run code if sensor not used, feed dummy 5.0 value to humidity
    
    float temperature_humidity[2] = {25.0, 50.0};

    while(1)
    {
        // Send humidity data to queue
        xQueueSend(task_queue, (void *) &temperature_humidity, (TickType_t) 5);
        // Delay f0r 500ms
        vTaskDelay(500/portTICK_PERIOD_MS);
    }

    #endif
}

// Main function
void app_main(void)
{
    // Float variable to store temperature and humidity
    float temperature_humidity[2];

    // Initialise queue for data transfer between tasks
    task_queue = xQueueCreate(5, sizeof(temperature_humidity));

    // Start task to receive data from SHT31 sensor
    xTaskCreatePinnedToCore(sht31_task, "SHT31 Sensor Task", 8192, NULL, 2, NULL, 1);

    // Run code if configured to test Rainmaker IoT service
    #if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_RAINMAKER_ONLY

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initialise Network
    app_network_init();

    // Configure Rainmaker
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };

    // Create Rainmaker node
    esp_rmaker_node_t * node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Drybox Device");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    // Initialise SHT31 device in Rainmaker application
    sht31_device = esp_rmaker_device_create("Drybox Humidity Sensor", NULL, &temperature_humidity);
    esp_rmaker_node_add_device(node, sht31_device);

    // Add the name of the device
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(sht31_device, 
                                esp_rmaker_param_create("Name", 
                                                        NULL, 
                                                        esp_rmaker_str("Drybox Humidity Sensor"),
                                                        PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST)));
    
    // Add Humidity as the primary parameter to the device
    esp_rmaker_param_t * humidity_param = esp_rmaker_param_create("Drybox Humidity", 
                                                                    NULL, 
                                                                    esp_rmaker_float(50.0), 
                                                                    PROP_FLAG_READ | PROP_FLAG_TIME_SERIES);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(humidity_param, ESP_RMAKER_UI_TEXT));
    esp_rmaker_param_add_bounds(humidity_param, esp_rmaker_float(0), esp_rmaker_float(100), esp_rmaker_float(0.1));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(sht31_device, humidity_param));
    ESP_ERROR_CHECK(esp_rmaker_device_assign_primary_param(sht31_device, humidity_param));

    // Add Temperature as another parameter
    esp_rmaker_param_t * temperature_param = esp_rmaker_temperature_param_create("Drybox Temperature", 25.0);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(temperature_param, ESP_RMAKER_UI_TEXT));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(sht31_device, temperature_param));

    // Enable OTA
    ESP_ERROR_CHECK(esp_rmaker_ota_enable_default());

    /* 
    Enable timezone service which will be require for setting appropriate timezone
    from the phone apps for scheduling to work correctly.
    For more information on the various ways of setting timezone, please check
    https://rainmaker.espressif.com/docs/time-service.html.

    esp_rmaker_timezone_service_enable();

    // Enable scheduling.
    esp_rmaker_schedule_enable();

    // Enable Scenes
    esp_rmaker_scenes_enable();*/

    // Enable Insights. Requires CONFIG_ESP_INSIGHTS_ENABLED=y
    ESP_ERROR_CHECK(app_insights_enable());

    // Start the ESP RainMaker Agent
    ESP_ERROR_CHECK(esp_rmaker_start());

    /* 
    Start the Wi-Fi.
    If the node is provisioned, it will start connection attempts,
    else, it will start Wi-Fi provisioning. The function will return
    after a connection has been successfully established
    */
    err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    #else // Run code if not testing IoT, prints out data from queue instead

    while(1)
    {
        // Check if the queue is not empty
        if(task_queue != 0)
        {
            // Dequeue recent message
            if(xQueueReceive(task_queue, &temperature_humidity, (TickType_t) 10))
            {
                // Print recent message
                ESP_LOGI(TAG, "Temperature: %f C, Humidity: %f%%", temperature_humidity[0], temperature_humidity[1]);
            }
        }   
        // Delay for 500ms
        vTaskDelay(500/portTICK_PERIOD_MS);
    }

    #endif
}
