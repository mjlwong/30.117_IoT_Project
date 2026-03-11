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

// Tag for log message
static const char * TAG = "main";

// Float variables to store temperature and humidity
static float temperature = 25.0, humidity = 40.0;

// Declare SHT31 sensor device object for interfacing
#if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_SENSOR_ONLY
static sht3x_t sht31_dev;
#endif

// Declare drybox device object for Rainmaker
#if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_RAINMAKER_ONLY 
esp_rmaker_device_t * drybox_device;
#endif

// Declare timer for how long the drybox has been too humid
TimerHandle_t humid_timer;

// Humid timer boolean variable to keep track if the timer has started yet
static bool timer_has_started = false;

// Add callback function if configured to test Rainmaker IoT service
#if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_RAINMAKER_ONLY

// Callback to handle commands received from the RainMaker cloud
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    // If there is text, display it
    if (ctx)
    {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }

    // If the user pressed the replaced silica gel button
    if (strcmp(esp_rmaker_param_get_name(param), "Press to indicate Silica Gel has been replaced") == 0)
    {
        // Set the dryness parameter to good on the app, stop the timer, clear the humid timer boolean to 0
        ESP_LOGI(TAG, "Silica Gel Replaced");
        esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(drybox_device, "Dryness Status"), 
                                            esp_rmaker_str("Good"));
        xTimerStop(humid_timer, 0);
        timer_has_started = false;
    }

    return ESP_OK;
}

#endif

// Humidity timer callback function
void dryness_update(TimerHandle_t humid_timer)
{
    // When time is up and humidity has not gone down
    if(humidity >= 60.0)
    {
        // Send warning on terminal that silica gel should be replaced
        ESP_LOGW(TAG, "High Humidity: Please replace Silica Gel");
        // Indicate on the app that the silica gel should be replaced
        esp_rmaker_param_update_and_notify(esp_rmaker_device_get_param_by_name(drybox_device, "Dryness Status"), 
                                            esp_rmaker_str("Bad: Please replace Silica Gel"));
    }

    // Reset humid timer boolean
    timer_has_started = false;
}

// SHT31 initialisation function
void sht31_init(void)
{
    // Run code if configured to use SHT31 sensor
    #if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_SENSOR_ONLY

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

    #endif
}

// Main function
void app_main(void)
{
    // Humidity Timer to go off if the drybox is humid
    humid_timer = xTimerCreate("Humidity Timer", 
                                (CONFIG_HUMID_TIMER_LENGTH * 1000 * 60) / portTICK_PERIOD_MS, 
                                pdFALSE, 
                                NULL, 
                                dryness_update);

    // Start task to receive data from SHT31 sensor
    sht31_init();

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
    if (!node)
    {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    // Initialise SHT31 device in Rainmaker application
    drybox_device = esp_rmaker_device_create("Drybox Humidity Sensor", NULL, NULL);
    esp_rmaker_device_add_cb(drybox_device, write_cb, NULL);
    esp_rmaker_node_add_device(node, drybox_device);

    // Add the name of the device
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(drybox_device, 
                                esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, 
                                                            "Drybox Humidity Sensor")));
    
    // Create a custom humidity parameter
    esp_rmaker_param_t * humidity_param = esp_rmaker_param_create("Drybox Humidity", 
                                                                NULL, 
                                                                esp_rmaker_float(50.0), 
                                                                PROP_FLAG_READ | PROP_FLAG_TIME_SERIES);
    // Humidity parameter to display the text of the humidity percentage
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(humidity_param, ESP_RMAKER_UI_TEXT));
    // Bounded between 0-100
    ESP_ERROR_CHECK(esp_rmaker_param_add_bounds(humidity_param, 
                                                esp_rmaker_float(0), 
                                                esp_rmaker_float(100), 
                                                esp_rmaker_float(0.1)));
    // Add the parameter to the device
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(drybox_device, humidity_param));
    // Add Humidity as the primary parameter to the device
    ESP_ERROR_CHECK(esp_rmaker_device_assign_primary_param(drybox_device, humidity_param));

    // Add Temperature as another parameter
    esp_rmaker_param_t * temperature_param = esp_rmaker_temperature_param_create("Drybox Temperature", 25.0);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(temperature_param, ESP_RMAKER_UI_TEXT));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(drybox_device, temperature_param));

    // Create boolean custom parameter to see if the drybox is adequetely dry
    esp_rmaker_param_t * dryness_param = esp_rmaker_param_create("Dryness Status", 
                                                                NULL, 
                                                                esp_rmaker_str("Good"), 
                                                                PROP_FLAG_READ);
    // Dryness parameter to display the text of the humidity percentage
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(dryness_param, ESP_RMAKER_UI_TEXT));
    // Add the parameter to the device
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(drybox_device, dryness_param));

    // Create boolean custom parameter to check if silica gel is replaced
    esp_rmaker_param_t * is_gel_replaced_param = esp_rmaker_param_create("Press to indicate Silica Gel has been replaced", 
                                                                        NULL, 
                                                                        esp_rmaker_bool(true), 
                                                                        PROP_FLAG_READ | PROP_FLAG_WRITE);
    // Trigger button to press when silica gel is replaced
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(is_gel_replaced_param, ESP_RMAKER_UI_TRIGGER));
    // Add the parameter to the device
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(drybox_device, is_gel_replaced_param));

    // Enable OTA
    ESP_ERROR_CHECK(esp_rmaker_ota_enable_default());

    /* 
    Enable timezone service which will be require for setting appropriate timezone
    from the phone apps for scheduling to work correctly.
    For more information on the various ways of setting timezone, please check
    https://rainmaker.espressif.com/docs/time-service.html.
    */
    ESP_ERROR_CHECK(esp_rmaker_timezone_service_enable());

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
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    #endif

    // Infinite loop for the programme with counter
    for(int counter = 0; true; counter++)
    {
        // Run code if configured to use SHT31 sensor
        #if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_SENSOR_ONLY

        // Get measurement from sensor
        ESP_ERROR_CHECK(sht3x_measure(&sht31_dev, &temperature, &humidity));

        #endif

        // When it is too humid and the timer has not started yet
        if(humidity >= 60.0 && !timer_has_started)
        {
            // Start the timer
            ESP_LOGI(TAG, "Timer started");
            xTimerStart(humid_timer, 0);

            // Set the humid timer boolean to true
            timer_has_started = true;
        }
        
        // When counter is 120 (1 min has passed)
        if(counter == 120)
        {
            // Run code if configured to test Rainmaker IoT service
            #if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_RAINMAKER_ONLY

            // Add code to update rainmaker cloud here
            ESP_ERROR_CHECK(esp_rmaker_param_update_and_report(humidity_param, esp_rmaker_float(humidity)));
            ESP_ERROR_CHECK(esp_rmaker_param_update_and_report(temperature_param, esp_rmaker_float(temperature)));

            #endif

            // Display updated temperature and humidity
            ESP_LOGI(TAG, "Temperature(C): %f, Humidity(%%): %f", temperature, humidity);
            counter = 0; // Clear counter
        }

        // Delay for 500 ms
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}
