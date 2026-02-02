#include "captive_portal.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include <string.h>

static const char *TAG = "CAPTIVE";
static int dns_socket = -1;
static TaskHandle_t dns_task_handle = NULL;
static bool running = false;

// Simple DNS packet structure
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answers;
    uint16_t authority;
    uint16_t additional;
} dns_header_t;

static void dns_server_task(void *pvParameters)
{
    char rx_buffer[CONFIG_DNS_MAX_LEN];
    char tx_buffer[CONFIG_DNS_MAX_LEN];
    struct sockaddr_in dest_addr;
    
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(CONFIG_DNS_PORT);
    
    dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (dns_socket < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        running = false;
        vTaskDelete(NULL);
        return;
    }
    
    int err = bind(dns_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(dns_socket);
        dns_socket = -1;
        running = false;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "DNS server started on port %d", CONFIG_DNS_PORT);
    
    while (running) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        
        int len = recvfrom(dns_socket, rx_buffer, CONFIG_DNS_MAX_LEN - 1, 0,
                          (struct sockaddr *)&source_addr, &socklen);
        
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }
        
        if (len < sizeof(dns_header_t)) {
            continue;  // Too short to be valid DNS
        }
        
        // Parse DNS header
        dns_header_t *header = (dns_header_t *)rx_buffer;
        
        // Only respond to standard queries
        if ((ntohs(header->flags) & 0x8000) != 0) {
            continue;  // This is a response, not a query
        }
        
        // Build DNS response - redirect everything to AP IP
        memcpy(tx_buffer, rx_buffer, len);
        dns_header_t *response = (dns_header_t *)tx_buffer;
        
        // Set response flags: QR=1 (response), AA=1 (authoritative), RCODE=0 (no error)
        response->flags = htons(0x8400);
        response->answers = htons(1);  // One answer
        
        // Skip to end of query section to add answer
        int pos = sizeof(dns_header_t);
        
        // Skip question name
        while (pos < len && rx_buffer[pos] != 0) {
            if ((rx_buffer[pos] & 0xC0) == 0xC0) {
                pos += 2;  // Compressed name pointer
                break;
            }
            pos += rx_buffer[pos] + 1;
        }
        pos++;  // Skip null terminator
        
        // Skip question type and class
        pos += 4;
        
        if (pos + 16 > CONFIG_DNS_MAX_LEN) {
            continue;  // Response would be too long
        }
        
        // Add answer: name pointer (to question), type A, class IN, TTL, length, IP
        tx_buffer[pos++] = 0xC0;  // Name pointer
        tx_buffer[pos++] = 0x0C;  // Points to offset 12 (start of question)
        
        tx_buffer[pos++] = 0x00;  // Type A (high byte)
        tx_buffer[pos++] = 0x01;  // Type A (low byte)
        
        tx_buffer[pos++] = 0x00;  // Class IN (high byte)
        tx_buffer[pos++] = 0x01;  // Class IN (low byte)
        
        // TTL: 60 seconds
        tx_buffer[pos++] = 0x00;
        tx_buffer[pos++] = 0x00;
        tx_buffer[pos++] = 0x00;
        tx_buffer[pos++] = 0x3C;
        
        // Data length: 4 bytes for IPv4
        tx_buffer[pos++] = 0x00;
        tx_buffer[pos++] = 0x04;
        
        // IP address: 192.168.4.1
        tx_buffer[pos++] = 192;
        tx_buffer[pos++] = 168;
        tx_buffer[pos++] = 4;
        tx_buffer[pos++] = 1;
        
        // Send response
        int err = sendto(dns_socket, tx_buffer, pos, 0,
                        (struct sockaddr *)&source_addr, sizeof(source_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        }
    }
    
    if (dns_socket >= 0) {
        close(dns_socket);
        dns_socket = -1;
    }
    
    ESP_LOGI(TAG, "DNS server stopped");
    vTaskDelete(NULL);
}

esp_err_t captive_portal_start(void)
{
    if (running) {
        ESP_LOGW(TAG, "Captive portal already running");
        return ESP_OK;
    }
    
    running = true;
    
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
    
    ESP_LOGI(TAG, "Captive portal started");
    return ESP_OK;
}

void captive_portal_stop(void)
{
    if (!running) {
        return;
    }
    
    running = false;
    
    if (dns_socket >= 0) {
        close(dns_socket);
        dns_socket = -1;
    }
    
    if (dns_task_handle) {
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Captive portal stopped");
}
