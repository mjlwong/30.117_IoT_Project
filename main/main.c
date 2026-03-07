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

// Declare SHT31 sensor device object for interfacing
#if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_SENSOR_ONLY
static sht3x_t sht31_dev;
#endif

// Declare SHT31 sensor device object for Rainmaker
#if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_RAINMAKER_ONLY 
esp_rmaker_device_t * sht31_device;
#endif

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

    // If the user pressed the replaced silica gel button, display corresponding text
    if (strcmp(esp_rmaker_param_get_name(param), "Press to indicate Silica Gel is replaced") == 0)
    {
        ESP_LOGI(TAG, "Silica Gel Replaced");
    }

    return ESP_OK;
}

#endif

// SHT31 task function
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
    // Float variables to store temperature and humidity
    float temperature = 25.0, humidity = 40.0;

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
    sht31_device = esp_rmaker_device_create("Drybox Humidity Sensor", NULL, NULL);
    esp_rmaker_device_add_cb(sht31_device, write_cb, NULL);
    esp_rmaker_node_add_device(node, sht31_device);

    // Add the name of the device
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(sht31_device, 
                                esp_rmaker_param_create("Name", 
                                                        NULL, 
                                                        esp_rmaker_str("Drybox Humidity Sensor"),
                                                        PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST)));
    
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
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(sht31_device, humidity_param));
    // Add Humidity as the primary parameter to the device
    ESP_ERROR_CHECK(esp_rmaker_device_assign_primary_param(sht31_device, humidity_param));

    // Add Temperature as another parameter
    esp_rmaker_param_t * temperature_param = esp_rmaker_temperature_param_create("Drybox Temperature", 25.0);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(temperature_param, ESP_RMAKER_UI_TEXT));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(sht31_device, temperature_param));

    // Create boolean custom parameter to see if the drybox is adequetely dry
    esp_rmaker_param_t * is_gel_replaced_param = esp_rmaker_param_create("Press to indicate Silica Gel is replaced", 
                                                                NULL, 
                                                                esp_rmaker_bool(true), 
                                                                PROP_FLAG_READ | PROP_FLAG_WRITE);
    // Dryness parameter to display the text of the humidity percentage
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(is_gel_replaced_param, ESP_RMAKER_UI_TRIGGER));
    // Add the parameter to the device
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(sht31_device, is_gel_replaced_param));

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

    while(1)
    {
        // Run code if configured to use SHT31 sensor
        #if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_SENSOR_ONLY

        // Get measurement from sensor
        ESP_ERROR_CHECK(sht3x_measure(&sht31_dev, &temperature, &humidity));

        #endif

        // Run code if configured to test Rainmaker IoT service
        #if defined CONFIG_BOTH_RAINMAKER_AND_SENSOR || defined CONFIG_RAINMAKER_ONLY

        // Add code to update rainmaker cloud here
        ESP_ERROR_CHECK(esp_rmaker_param_update_and_report(humidity_param, esp_rmaker_float(humidity)));
        ESP_ERROR_CHECK(esp_rmaker_param_update_and_report(temperature_param, esp_rmaker_float(temperature)));

        #endif

        ESP_LOGI(TAG, "Temperature(C): %f, Humidity(%%): %f", temperature, humidity);

        // Delay for 1 min
        vTaskDelay(60000/portTICK_PERIOD_MS);
    }
}
