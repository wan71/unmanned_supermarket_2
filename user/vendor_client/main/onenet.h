#ifndef __ONENET_H__
#define __ONENET_H__
#include "ping/ping_sock.h"

#define ESP_MQTT_URI "mqtt://mqtts.heclouds.com"
#define ESP_MQTT_PORT 1883
#define ESP_CLIENT_ID  "esp32c3_1"//设备id
#define ESP_PRODUCT_ID "qWeNMxV7Vj"//产品id
#define ESP_MQTT_TOKEN "version=2018-10-31&res=products%2FqWeNMxV7Vj%2Fdevices%2Fesp32c3_1&et=2041489578&method=md5&sign=YHBdhyOF2fCwDoMBzTiReA%3D%3D"//TOKEN
                     




// //这是post上传数据使用的模板
// #define ONENET_POST_BODY_FORMAT "{\"id\":123,\"dp\":%s}"

#define ONENET_TOPIC_SUB "$sys/qWeNMxV7Vj/esp32c3_1/thing/property/post/reply"  //订阅
#define ONENET_TOPIC_PUBLISH "$sys/qWeNMxV7Vj/esp32c3_1/thing/property/post" //上报

//MQTT状态
typedef enum {
    MQTT_STATE_ERROR = -1,
    MQTT_STATE_UNKNOWN = 0,
    MQTT_STATE_INIT,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_WAIT_TIMEOUT,
} my_mqtt_client_state_t;

void initialize_ping();
void mqtt_app_start(void);
int mqtt_data_publish(const char* themes[], const void* values[], const char* value_types[], int count);

#endif