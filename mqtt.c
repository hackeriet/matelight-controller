#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <mosquitto.h>

#include "game.h"

#define MQTT_TOPIC "hackeriet/ding"
#define CA_CERTIFICATES "/etc/ssl/certs/ca-certificates.crt"

static const char *mqtt_server = "localhost";
static int mqtt_port = 1883;
static bool mqtt_tls = false;
static const char *mqtt_username = NULL;
static const char *mqtt_password = NULL;

static pthread_t mqtt_thread;
static struct mosquitto *mosq;

static void on_log(struct mosquitto *mosq, void *obj, int level, const char *str)
{
    (void)mosq;
    (void)obj;

    fprintf(stderr, "mqtt: on_log: %d: %s\n", level, str);
}

static void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
    int rc;

    (void)obj;

    fprintf(stderr, "mqtt: on_connect: %s\n", mosquitto_connack_string(reason_code));
    if (reason_code != 0) {
        //mosquitto_disconnect(mosq);
        return;
    }

    rc = mosquitto_subscribe(mosq, NULL, MQTT_TOPIC, 1);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "mqtt: Error subscribing: %s\n", mosquitto_strerror(rc));
        mosquitto_disconnect(mosq);
        return;
    }
}

static void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
    int i;
    bool have_subscription = false;

    (void)obj;
    (void)mid;

    for (i = 0; i < qos_count; i++) {
        fprintf(stderr, "mqtt: on_subscribe: %d:granted qos = %d\n", i, granted_qos[i]);
        if (granted_qos[i] <= 2){
            have_subscription = true;
        }
    }
    if (! have_subscription) {
        fprintf(stderr, "mqtt: Error: All subscriptions rejected.\n");
        mosquitto_disconnect(mosq);
        return;
    }
}

static void strip_garbage(char *text)
{
    char *garbage = NULL, *s;
    size_t len;

    if (! text || !*text)
        return;

    len = strlen(text);
    if (len <= 0 || text[len - 1] != '>')
        return;

    for (;;) {
        if (garbage) {
            s = strstr(garbage + 2, " <");
        } else {
            s = strstr(text, " <");
        }
        if (s) {
            garbage = s;
        } else {
            break;
        }
    }

    if (garbage)
        *garbage = '\0';
}

static void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
    char *text;
    (void)mosq;
    (void)obj;

    fprintf(stderr, "mqtt: on_message: %s %d %d:%.*s\n", msg->topic, msg->qos, msg->payloadlen, msg->payloadlen, (char *)msg->payload);

    if (msg->topic && strcmp(msg->topic, MQTT_TOPIC) == 0 && msg->payloadlen > 0) {
        text = malloc(msg->payloadlen + 1);
        if (text) {
            memcpy(text, msg->payload, msg->payloadlen);
            text[msg->payloadlen] = '\0';
            strip_garbage(text);
            do_announce_async(text, COLOR_BLACK, COLOR_YELLOW, 1, 5.0);
        }
    }
}

static void *mqtt_thread_func(void *arg)
{
    int rc;

    (void)arg;

    mosquitto_lib_init();

    mosq = mosquitto_new(NULL, true, NULL);
    if (! mosq) {
        mosquitto_lib_cleanup();
        return NULL;
    }

    mosquitto_log_callback_set(mosq, on_log);
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_subscribe_callback_set(mosq, on_subscribe);
    mosquitto_message_callback_set(mosq, on_message);

    if (mqtt_tls) {
        mosquitto_tls_set(mosq, CA_CERTIFICATES, NULL, NULL, NULL, NULL);
        mosquitto_tls_insecure_set(mosq, true);
    }
    if (mqtt_username && *mqtt_username) {
        mosquitto_username_pw_set(mosq, mqtt_username, (mqtt_password && *mqtt_password) ? mqtt_password : NULL);
    }
    rc = mosquitto_connect(mosq, mqtt_server, mqtt_port, 60);
    if (rc != MOSQ_ERR_SUCCESS){
        mosquitto_destroy(mosq);
        fprintf(stderr, "mqtt: %s\n", mosquitto_strerror(rc));
        mosquitto_lib_cleanup();
        return NULL;
    }

    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_lib_cleanup();
    return NULL;
}

static void mqtt_configure(void)
{
    char *str;

    str = getenv("MQTT_SERVER");
    if (str && *str) {
        mqtt_server = str;
    } else {
        mqtt_server = "localhost";
    }

    str = getenv("MQTT_TLS");
    if (str && *str && strcmp(str, "0") != 0) {
        mqtt_tls = true;
    } else {
        mqtt_tls = false;
    }

    str = getenv("MQTT_PORT");
    if (str && *str && strcmp(str, "0") != 0) {
        mqtt_port = atoi(str);
    } else {
        if (mqtt_tls) {
            mqtt_port = 8883;
        } else {
            mqtt_port = 1883;
        }
    }

    str = getenv("MQTT_USERNAME");
    if (str && *str) {
        mqtt_username = str;
    } else {
        mqtt_username = NULL;
    }

    str = getenv("MQTT_PASSWORD");
    if (str && *str) {
        mqtt_password = str;
    } else {
        mqtt_password = NULL;
    }
}

void mqtt_init(void)
{
    mqtt_configure();

    if (pthread_create(&mqtt_thread, NULL, mqtt_thread_func, NULL) != 0) {
        perror("pthread_create");
        return;
    }

    (void)pthread_detach(mqtt_thread);
}
