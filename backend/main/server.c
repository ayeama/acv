#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_random.h"
#include "esp_check.h"
#include "esp_vfs.h"

#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_http_server.h"

#include <acv.h>

#define SERVER_MOUNT_POINT "/www"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define CHECK_FILE_EXTENSION(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

#define SCRATCH_BUFSIZE (10240)

#define MAX_WS_CLIENTS 8

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

static httpd_handle_t ws_server = NULL;

static int ws_clients[MAX_WS_CLIENTS] = {0};

static void ws_add_client(int fd) {
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_clients[i] == 0) {
            ws_clients[i] = fd;
            ESP_LOGI("acv", "Added websocket client fd=%d", fd);
            return;
        }
    }

    ESP_LOGW("acv", "No free websocket client slots");
}

static void ws_remove_client(int fd) {
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_clients[i] == fd) {
            ws_clients[i] = 0;
            ESP_LOGI("acv", "Removed websocket client fd=%d", fd);
            return;
        }
    }
}

static void websocket_broadcast_task(void *arg) {
    char json[128];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30));

        if (!acv_msg.updated) {
            continue;
        }
        acv_msg.updated = false;

        acv_msg_string(json, sizeof(json));

        httpd_ws_frame_t ws_pkt = {
            .final = true,
            .fragmented = false,
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t *)json,
            .len = strlen(json)
        };

        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            int fd = ws_clients[i];

            if (fd == 0) {
                continue;
            }

            esp_err_t ret = httpd_ws_send_frame_async(ws_server, fd, &ws_pkt);

            if (ret != ESP_OK) {
                ESP_LOGW("acv", "Failed sending websocket frame to fd=%d (%s)", fd, esp_err_to_name(ret));
                ws_clients[i] = 0;
            }
        }
    }
}

esp_err_t init_fs() {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = SERVER_MOUNT_POINT,
        .partition_label = "www",
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("acv", "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("acv", "Failed to find LittleFS partition");
        } else {
            ESP_LOGE("acv", "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }

        return ESP_FAIL;
    }

    size_t total = 0;
    size_t used = 0;

    ret = esp_littlefs_info(conf.partition_label, &total, &used);

    if (ret != ESP_OK) {
        ESP_LOGE("acv", "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        esp_littlefs_format(conf.partition_label);
    } else {
        ESP_LOGI("acv", "Partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
}

static esp_err_t websocket_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        int fd = httpd_req_to_sockfd(req);
        ws_add_client(fd);
        ESP_LOGI("acv", "WebSocket client connected fd=%d", fd);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);

    if (ret != ESP_OK) {
        ws_remove_client(httpd_req_to_sockfd(req));
        ESP_LOGE("acv", "httpd_ws_recv_frame failed (%s)", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.len > 0) {
        uint8_t *buf = calloc(1, ws_pkt.len + 1);

        if (buf == NULL) {
            ESP_LOGE("acv", "Failed to allocate websocket buffer");

            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);

        if (ret != ESP_OK) {
            free(buf);
            ws_remove_client(httpd_req_to_sockfd(req));
            return ret;
        }

        ESP_LOGI("acv", "Received websocket message: %s", (char *)ws_pkt.payload);
        free(buf);
    }

    return ESP_OK;
}

static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath) {
    const char *type = "text/plain";

    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "image/svg+xml";
    }

    return httpd_resp_set_type(req, type);
}

static esp_err_t rest_common_get_handler(httpd_req_t *req) {
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context =
        (rest_server_context_t *)req->user_ctx;

    strlcpy(filepath, rest_context->base_path, sizeof(filepath));

    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }

    int fd = open(filepath, O_RDONLY, 0);

    if (fd == -1) {
        ESP_LOGE("acv", "Failed to open file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);
    char *chunk = rest_context->scratch;
    ssize_t read_bytes;

    do {
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);

        if (read_bytes == -1) {
            ESP_LOGE("acv", "Failed to read file: %s", filepath);
        } else if (read_bytes > 0) {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE("acv", "File sending failed");
                httpd_resp_sendstr_chunk(req, NULL);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);

    close(fd);
    // ESP_LOGI("acv", "File sending complete");
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        base_path && strlen(base_path) < ESP_VFS_PATH_MAX,
        ESP_ERR_INVALID_ARG,
        "acv",
        "Invalid base path"
    );

    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    ESP_RETURN_ON_FALSE(
        rest_context,
        ESP_ERR_NO_MEM,
        "acv",
        "No memory for rest context"
    );

    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI("acv", "Starting HTTP Server");
    ESP_GOTO_ON_ERROR(
        httpd_start(&server, &config),
        err,
        "acv",
        "Failed to start http server"
    );

    ws_server = server;
    
    httpd_uri_t websocket_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = websocket_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };
    httpd_register_uri_handler(server, &websocket_uri);
    
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);

    xTaskCreate(websocket_broadcast_task, "ws_broadcast", 4096, NULL, 5, NULL);

    return ESP_OK;

err:
    if (rest_context) {
        free(rest_context);
    }

    return ret;
}

void initialize_server() {
    ESP_ERROR_CHECK(init_fs());
    ESP_ERROR_CHECK(start_rest_server(SERVER_MOUNT_POINT));
}