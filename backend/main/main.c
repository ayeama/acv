#include "mdns.h"

#include <acv.h>
#include "nvs.c"
#include "server.c"
#include "twai.c"
#include "wifi.c"

#define MDNS_HOST_NAME "acv"
#define MDNS_INSTANCE "Automotive CAN bus Visualiser"

static void initialise_mdns() {
    mdns_init();
    mdns_hostname_set(MDNS_HOST_NAME);
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"chip", CONFIG_IDF_TARGET},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add(
        NULL,
        "_http",
        "_tcp",
        80,
        serviceTxtData,
        sizeof(serviceTxtData) / sizeof(serviceTxtData[0])
    ));
}

void app_main() {
    initialize_acv();

    initialize_nvs();
    initialize_wifi();
    initialise_mdns();
    initialize_twai();
    initialize_server();
}
