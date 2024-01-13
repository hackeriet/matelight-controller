#include <stddef.h>
#include <stdlib.h>

#include <mosquitto.h>

#include "game.h"

void mqtt_init(void)
{
    mosquitto_lib_init();
    mosquitto_lib_cleanup();
}
