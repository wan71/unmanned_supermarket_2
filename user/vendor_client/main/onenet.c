#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "onenet.h"

#include "esp_ping.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"

#include "ping/ping_sock.h" // 确保包含正确的头文件
#include "lwip/inet.h"

#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <netdb.h>
#include "mqtt5_client.h"

// #include "mqtt_client_priv.h"

esp_mqtt_client_handle_t client;
static const char *TAG = "ONENET_MQTT";
bool mqtt_connect_flag=false;//连接标志位 是否连接成功
static my_mqtt_client_state_t mqtt_sta = MQTT_STATE_INIT;//MQTT状态

static void test_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    // optionally, get callback arguments
    // const char* str = (const char*) args;
    // printf("%s\r\n", str); // "foo"
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("%ld bytes from %s icmp_seq=%d ttl=%d time=%ld ms\n",
           recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
}

static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    printf("From %s icmp_seq=%d timeout\n", inet_ntoa(target_addr.u_addr.ip4), seqno);
}

static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    printf("%ld packets transmitted, %ld received, time %ldms\n", transmitted, received, total_time_ms);
}

void initialize_ping()
{
    /* convert URL to IP address */
    ip_addr_t target_addr;
    struct addrinfo hint;
    struct addrinfo *res = NULL;
    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));
    // getaddrinfo("www.espressif.com", NULL, &hint, &res);
    getaddrinfo("www.baidu.com", NULL, &hint, &res);
    struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    freeaddrinfo(res);

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;          // target IP address
    ping_config.count = ESP_PING_COUNT_INFINITE;    // ping in infinite mode, esp_ping_stop can stop it

    /* set callback functions */
    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = test_on_ping_success;
    cbs.on_ping_timeout = test_on_ping_timeout;
    cbs.on_ping_end = test_on_ping_end;
    cbs.cb_args = NULL;  // arguments that will feed to all callback functions, can be NULL
    // cbs.cb_args = eth_event_group;

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    esp_ping_start(ping);  // 开始 ping 操作
}

// static bool mqtt_connect_flag = false;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    // const char* themes[] = {"hum", "temp"};
    // float hum_value = 22.0;
    // float temp_value = 25.0;
    // const void* values[] = {&hum_value, &temp_value};
    // const char* value_types[] = {"float", "float"};

    int msg_id;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id=esp_mqtt_client_subscribe(event->client, ONENET_TOPIC_SUB, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        if(msg_id != -1)
            {
                mqtt_connect_flag=true;
                mqtt_sta =MQTT_STATE_CONNECTED;
            }else
            {
                mqtt_connect_flag=false;
                mqtt_sta =MQTT_STATE_UNKNOWN;
            }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        // msg_id =mqtt_data_publish(themes, values, value_types, 2);
        // msg_id = mqtt_data_publish("mylog","suss");
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        mqtt_connect_flag=true;
        mqtt_sta =MQTT_STATE_CONNECTED;
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
            mqtt_connect_flag=false;
            mqtt_sta =MQTT_STATE_UNKNOWN;
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
        break;

    default:
        ESP_LOGI(TAG, "Unhandled event: %d", event->event_id);
        break;
    }
    return ESP_OK;
}

 

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void mqtt_app_start(void)
{

        esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = ESP_MQTT_URI,  // 使用安全的MQTT连接（mqtts）
        .broker.address.port = 1883,                      // TLS端口
        .credentials.client_id = ESP_CLIENT_ID,//设备id         
        .credentials.username = ESP_PRODUCT_ID,//产品id         
        .credentials.authentication.password = ESP_MQTT_TOKEN,//TOKEN

    };

    // 初始化MQTT客户端
    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "MQTT client initialization failed");
        return;
    }
    // 注册事件
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);

    // 启动MQTT客户端
    esp_mqtt_client_start(client);
}



/*
    param: themes[]  物模型名数组
    param: values[]  上传数据数组（可以是浮点型或整型）
    param: value_types[]  上传数据的类型（"int" 或 "float"）
    param: count     物模型的数量
*/

int mqtt_data_publish(const char* themes[], const void* values[], const char* value_types[], int count)
{
    char data_str[512];
    int msg_id = -1;
    
    // 初始化数据包的开头部分
    snprintf(data_str, sizeof(data_str), "{\"id\": \"123\",\"version\": \"1.0\",\"params\": {");
    
    // 循环拼接物模型名和对应的值
    for (int i = 0; i < count; i++)
    {
        char param_str[128];
        
        // 根据值的类型进行格式化拼接
        if (strcmp(value_types[i], "int") == 0) {
            snprintf(param_str, sizeof(param_str), "\"%s\":{\"value\":%d}", themes[i], *(int*)values[i]);
        } else if (strcmp(value_types[i], "float") == 0) {
            snprintf(param_str, sizeof(param_str), "\"%s\":{\"value\":%.1f}", themes[i], *(float*)values[i]);
        } else if (strcmp(value_types[i], "string") == 0) {
            snprintf(param_str, sizeof(param_str), "\"%s\":{\"value\":\"%s\"}", themes[i], (char*)values[i]);
        }
        
        // 拼接到data_str中，并确保不是最后一个元素时加上逗号
        strncat(data_str, param_str, sizeof(data_str) - strlen(data_str) - 1);
        if (i < count - 1) {
            strncat(data_str, ",", sizeof(data_str) - strlen(data_str) - 1);
        }
    }
    
    // 结束数据包的组装
    strncat(data_str, "}}", sizeof(data_str) - strlen(data_str) - 1);
    printf("%s\n", data_str);
    
    // 发送数据包
    if (mqtt_connect_flag == true && client != NULL)
    {
        msg_id = esp_mqtt_client_publish(client, ONENET_TOPIC_PUBLISH, data_str, 0, 0, 0);
        ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
        
        if (msg_id != -1)
        {
            ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to publish message");
        }
    }
    else
    {
        ESP_LOGW(TAG, "MQTT client is not connected");
    }
    
    return msg_id;
}
/*
int mqtt_data_publish(const char* themes[], const void* values[], const char* value_types[], int count)
{
    char data_str[512];
    int msg_id = -1;
    
    // 初始化数据包的开头部分
    snprintf(data_str, sizeof(data_str), "{\"id\": \"123\",\"version\": \"1.0\",\"params\": {");
    
    // 循环拼接物模型名和对应的值
    for (int i = 0; i < count; i++)
    {
        char param_str[128];
        
        // 根据值的类型进行格式化拼接
        if (strcmp(value_types[i], "int") == 0) {
            snprintf(param_str, sizeof(param_str), "\"%s\":{\"value\":%d}", themes[i], *(int*)values[i]);
        } else if (strcmp(value_types[i], "float") == 0) {
            snprintf(param_str, sizeof(param_str), "\"%s\":{\"value\":%.1f}", themes[i], *(float*)values[i]);
        }
        
        // 拼接到data_str中，并确保不是最后一个元素时加上逗号
        strncat(data_str, param_str, sizeof(data_str) - strlen(data_str) - 1);
        if (i < count - 1) {
            strncat(data_str, ",", sizeof(data_str) - strlen(data_str) - 1);
        }
    }
    
    // 结束数据包的组装
    strncat(data_str, "}}", sizeof(data_str) - strlen(data_str) - 1);
    printf("%s\n", data_str);
    if (mqtt_connect_flag == true && client != NULL)
    {
        msg_id = esp_mqtt_client_publish(client, ONENET_TOPIC_PUBLISH, data_str, 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        
        if (msg_id != -1)
        {
            ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to publish message");
        }
    }
    else
    {
        ESP_LOGW(TAG, "MQTT client is not connected");
    }
    
    return msg_id;
}
*/