#include "tcp_client.h"
#include <errno.h>
#include <lwip/netdb.h>
#include <string.h>
#include <sys/param.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#include "bt_receiver.h"
#include "ld_nvs.h"
#include "readframe.h"
#include "sd_writer.h"

static const char* TAG = "TCP_CLIENT";

/* Wi-Fi Event Group */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define TCP_BUFFER_SIZE 16384

static int s_retry_num = 0;

extern QueueHandle_t sys_cmd_queue;
static esp_netif_t* s_wifi_netif = NULL;
static esp_event_handler_instance_t instance_any_id = NULL;
static esp_event_handler_instance_t instance_got_ip = NULL;
static bool s_is_stopping = false;

/* Wi-Fi Event Handler */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if(!s_is_stopping && s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Wi-Fi disconnected; retrying connection: attempt=%d maximum=5", s_retry_num);
        } else {
            ESP_LOGE(TAG, "Wi-Fi connection failed after %d retries", s_retry_num);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Wi-Fi connected: ip=" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* Wi-Fi Initialization */
static void wifi_init_sta(void) {
    // Create the event group to handle Wi-Fi events
    s_wifi_event_group = xEventGroupCreate();

    // Init TCP/IP stack and event loop
    esp_netif_init();
    esp_event_loop_create_default();

    // Create default Wi-Fi station
    if(s_wifi_netif == NULL) {
        s_wifi_netif = esp_netif_create_default_wifi_sta();
    }

    // Register event handlers for Wi-Fi and IP events
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Register event handlers for Wi-Fi and IP events
    if(instance_any_id == NULL) {
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id);
    }

    // Register the event handler for when the device gets an IP address
    if(instance_got_ip == NULL) {
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip);
    }

    // Reset retry count and flags
    s_retry_num = 0;
    s_is_stopping = false;

    // Configure Wi-Fi connection settings
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, TCP_WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, TCP_WIFI_PASS);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    // Set Wi-Fi mode to station and start Wi-Fi
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_start();

    ESP_LOGI(TAG, "Wi-Fi station started: ssid=%s", TCP_WIFI_SSID);
}

/* Helper function to receive exact number of bytes */
static int recv_exact(int sock, void* buf, size_t len) {
    size_t received = 0;
    while(received < len) {
        int ret = recv(sock, (char*)buf + received, len - received, 0);
        if(ret <= 0) {
            return ret;
        }
        received += ret;
    }
    return received;
}

/* Process to download a file from TCP server and save it to SPIFFS. */
static esp_err_t download_file(int sock, const char* filename) {
    uint32_t net_size = 0;

    // 1. Receive file size (4 bytes, network byte order)
    int size_result = recv_exact(sock, &net_size, sizeof(net_size));
    if(size_result <= 0) {
        ESP_LOGE(TAG, "file size receive failed: path=%s socket_result=%d errno=%d (%s)", filename, size_result, errno, strerror(errno));
        return ESP_FAIL;
    }
    uint32_t file_size = ntohl(net_size);
    ESP_LOGI(TAG, "file download started: path=%s size=%lu bytes", filename, (unsigned long)file_size);

    uint8_t* buf = (uint8_t*)malloc(TCP_BUFFER_SIZE);
    if(buf == NULL) {
        ESP_LOGE(TAG, "download buffer allocation failed: size=%u bytes path=%s", (unsigned)TCP_BUFFER_SIZE, filename);
        return ESP_ERR_NO_MEM;
    }

#if LD_CFG_ENABLE_PT
    // 2. Initialize the SPIFFS writer.
    esp_err_t writer_err = sd_writer_init(filename);
    if(writer_err != ESP_OK) {
        ESP_LOGE(TAG, "file writer initialization failed: path=%s err=%s", filename, esp_err_to_name(writer_err));
        free(buf);
        return writer_err;
    }
#else
    ESP_LOGI(TAG, "persistent storage is disabled; receiving file without writing: path=%s", filename);
#endif

    // 3. Receive file data in chunks
    size_t remaining = file_size;
    while(remaining > 0) {
        size_t to_read = (remaining < TCP_BUFFER_SIZE) ? remaining : TCP_BUFFER_SIZE;
        int n = recv_exact(sock, buf, to_read);
        if(n <= 0) {
            ESP_LOGE(TAG, "file data receive failed: path=%s remaining=%u bytes socket_result=%d errno=%d (%s)", filename, (unsigned)remaining, n, errno, strerror(errno));
#if LD_CFG_ENABLE_PT
            sd_writer_close();
#endif
            free(buf);
            return ESP_FAIL;
        }

#if LD_CFG_ENABLE_PT
        // Perform the SPIFFS write.
        writer_err = sd_writer_write(buf, n);
        if(writer_err != ESP_OK) {
            ESP_LOGE(TAG, "file write failed during download: path=%s chunk_size=%d remaining=%u err=%s", filename, n, (unsigned)remaining, esp_err_to_name(writer_err));
            sd_writer_close();
            free(buf);
            return ESP_FAIL;
        }
#else
        // Mock write delay (optional, slightly slow down reception to prevent buffer overload)
        // vTaskDelay(pdMS_TO_TICKS(5));
#endif
        remaining -= n;
    }

#if LD_CFG_ENABLE_PT
    sd_writer_close();
#endif
    free(buf);
    ESP_LOGI(TAG, "file download completed: path=%s size=%lu bytes", filename, (unsigned long)file_size);
    return ESP_OK;
}

/* Update Task Function */
static void update_task_func(void* pvParameters) {
    bool download_succeeded = false;
    ESP_LOGI(TAG, "content update task started");

    // [Step 1] Deinit BLE
    ESP_LOGD(TAG, "update step 1/3: stopping Bluetooth receiver");
    bt_receiver_deinit();
    vTaskDelay(pdMS_TO_TICKS(500));

    // [Step 2] Start Wi-Fi
    ESP_LOGD(TAG, "update step 2/3: starting Wi-Fi station");
    wifi_init_sta();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if(bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connecting to content server: address=%s port=%d", TCP_SERVER_IP, TCP_SERVER_PORT);

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(TCP_SERVER_IP);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(TCP_SERVER_PORT);

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if(sock < 0) {
            ESP_LOGE(TAG, "TCP socket creation failed: errno=%d (%s)", errno, strerror(errno));
        } else {
            int err = connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            if(err != 0) {
                ESP_LOGE(TAG, "content server connection failed: address=%s port=%d errno=%d (%s)", TCP_SERVER_IP, TCP_SERVER_PORT, errno, strerror(errno));
            } else {
                ESP_LOGI(TAG, "content server connected: address=%s port=%d", TCP_SERVER_IP, TCP_SERVER_PORT);

                // Optimize socket for bulk receive throughput (mirrors tcp_client reference)
                int rcvbuf = 65536;
                setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
                int nodelay = 1;
                setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

                // [Step 3] Message Player ID
#if LD_CFG_ENABLE_PT
                uint8_t stored_pid = 0;
                int pid = ld_nvs_get_player_id(&stored_pid) == ESP_OK ? stored_pid : 1;
                if(pid <= 0)
                    pid = 1;  // Fallback protection
#else
                int pid = 1;  // Force ID as 1 during test without SD card
#endif
                char msg[32];
                snprintf(msg, sizeof(msg), "%d\n", pid);
                int sent = send(sock, msg, strlen(msg), 0);
                if(sent != (int)strlen(msg)) {
                    ESP_LOGE(TAG, "player ID send failed: id=%d sent=%d expected=%u errno=%d (%s)", pid, sent, (unsigned)strlen(msg), errno, strerror(errno));
                } else {
                    ESP_LOGD(TAG, "player ID sent: id=%d", pid);
                }

                // [Step 4] Download Files
                ESP_LOGD(TAG, "update step 3/3: downloading content files");
                esp_err_t control_err = download_file(sock, "/spiffs/control.dat");
                esp_err_t frame_err = ESP_FAIL;
                if(control_err == ESP_OK) {
                    frame_err = download_file(sock, "/spiffs/frame.dat");
                }
                download_succeeded = control_err == ESP_OK && frame_err == ESP_OK;
                if(!download_succeeded) {
                    ESP_LOGE(TAG, "content download incomplete: control_err=%s frame_err=%s", esp_err_to_name(control_err), esp_err_to_name(frame_err));
                }
                const char* ack_msg = "DONE\n";
                send(sock, ack_msg, strlen(ack_msg), 0);
            }
            close(sock);
        }
    } else {
        ESP_LOGE(TAG, "content update aborted: Wi-Fi connection unavailable");
    }

    if(sys_cmd_queue != NULL) {
        sys_cmd_t msg = UPLOAD_SUCCESS;
        if(xQueueSend(sys_cmd_queue, &msg, 0) == pdTRUE) {
            ESP_LOGD(TAG, "update completion command queued: download_status=%s", download_succeeded ? "success" : "failed");
        } else {
            ESP_LOGE(TAG, "update completion command enqueue failed: system command queue is full");
        }
    } else {
        ESP_LOGE(TAG, "update completion command could not be queued: system command queue is NULL");
    }

    if(download_succeeded) {
        ESP_LOGI(TAG, "content update task completed successfully");
    } else {
        ESP_LOGE(TAG, "content update task completed with failure");
    }
    vTaskDelete(NULL);
}

void tcp_client_start_update_task(void) {
    if(xTaskCreate(update_task_func, "tcp_update", 16384, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "content update task creation failed: stack_size=16384 priority=5");
    }
}
