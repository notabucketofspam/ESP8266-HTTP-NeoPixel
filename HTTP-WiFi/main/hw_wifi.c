#include "hw_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Event group mostly for waiting until IP assignment
 */
static EventGroupHandle_t xWifiEventGroup = NULL;
/*
 * Used for setting the static IP address
 */
static tcpip_adapter_ip_info_t xIpInfoSta;
/*
 * How many times connection has been attempted
 */
static uint32_t ulRetryConnectionCounter = 0;
/*
 * Function prototypes
 */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void vHwSetupWifi(void) {
  // Setup TCPIP for having a static IP later on, if positioned to do so in Kconfig
  #if CONFIG_HW_STATIC_IP_ADDR
    xIpInfoSta.ip.addr = PP_HTONL(LWIP_MAKEU32(CONFIG_HW_IP_ADDR_1, CONFIG_HW_IP_ADDR_2,\
      CONFIG_HW_IP_ADDR_3, CONFIG_HW_IP_ADDR_4));
    xIpInfoSta.netmask.addr = PP_HTONL(LWIP_MAKEU32(CONFIG_HW_NETMASK_ADDR_1, CONFIG_HW_NETMASK_ADDR_2,\
      CONFIG_HW_NETMASK_ADDR_3, CONFIG_HW_NETMASK_ADDR_4));
    xIpInfoSta.gw.addr = PP_HTONL(LWIP_MAKEU32(CONFIG_HW_GW_ADDR_1, CONFIG_HW_GW_ADDR_2,\
      CONFIG_HW_GW_ADDR_3, CONFIG_HW_GW_ADDR_4));
  #endif
  // Basic init stuff, refer to the Espressif examples for what's going on
  esp_event_loop_create_default();
  nvs_flash_init();
  xWifiEventGroup = xEventGroupCreate();
  tcpip_adapter_init();
  // Reading the MAC is a bit frivolous now, but useful for debugging
  uint8_t pcMac[6];
  esp_wifi_get_mac(ESP_IF_WIFI_STA, pcMac);
  ESP_LOGI(__ESP_FILE__, "pcMac "MACSTR, MAC2STR(pcMac));
  // WiFi init stuff
  wifi_init_config_t xWifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&xWifiInitConfig);
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
  esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
  wifi_config_t xWifiConfig = {
    .sta = {
      .ssid = CONFIG_HW_WIFI_SSID,
      .password = CONFIG_HW_WIFI_PASSWORD,
    }
  };
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(ESP_IF_WIFI_STA, &xWifiConfig);
  esp_wifi_set_protocol(ESP_IF_WIFI_STA, CONFIG_HW_WIFI_PROTOCOL);
  esp_wifi_start();
  xEventGroupWaitBits(xWifiEventGroup, HW_BIT_WIFI_STA_START | HW_BIT_WIFI_STA_CONNECT | HW_BIT_WIFI_IP_GET, pdTRUE,
    pdTRUE, pdMS_TO_TICKS(1000 * CONFIG_HW_WIFI_TIMEOUT));
}
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    // Connect once WiFi has fully started
    xEventGroupSetBits(xWifiEventGroup, HW_BIT_WIFI_STA_START);
    ESP_LOGI(__ESP_FILE__, "STA start");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    xEventGroupSetBits(xWifiEventGroup, HW_BIT_WIFI_STA_CONNECT);
    ESP_LOGI(__ESP_FILE__, "STA connect");
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    // Make an attempt to reconnect, but give up if it's futile
    ESP_LOGW(__ESP_FILE__, "STA disconnect");
    if (ulRetryConnectionCounter < CONFIG_HW_WIFI_RETRY_MAX) {
      esp_wifi_connect();
      ++ulRetryConnectionCounter;
    } else {
      ESP_LOGE(__ESP_FILE__, "STA connect fail, restart");
      esp_restart();
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(xWifiEventGroup, HW_BIT_WIFI_IP_GET);
    ulRetryConnectionCounter = 0;
    ip_event_got_ip_t* pxEvent = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(__ESP_FILE__, "pxEvent->ip_changed=%d", (int) pxEvent->ip_changed);
    // Only do the thing if IP has changed; prevents an infinite loop of "Oh
    // look, a new IP! Let's do the thing!" from happening.
    if (pxEvent->ip_changed) {
      ESP_LOGI(__ESP_FILE__, "IP get: %s", ip4addr_ntoa(&pxEvent->ip_info.ip));
      // Shutdown DHCPC so that a static IP can be assigned
      #if CONFIG_HW_STATIC_IP_ADDR
        tcpip_adapter_dhcp_status_t xDhcpcStatus;
        tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &xDhcpcStatus);
        if (xDhcpcStatus == TCPIP_ADAPTER_DHCP_STARTED) {
          tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
        }
        tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &xIpInfoSta);
      #endif
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
    ESP_LOGI(__ESP_FILE__, "IP loss");
    // Startup DHCPC again so that a new IP address can be obtained
    #if CONFIG_HW_STATIC_IP_ADDR
      tcpip_adapter_dhcp_status_t xDhcpcStatus;
      tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &xDhcpcStatus);
      if (xDhcpcStatus == TCPIP_ADAPTER_DHCP_STOPPED) {
        tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
      }
    #endif
  } // if
}

#ifdef __cplusplus
}
#endif
