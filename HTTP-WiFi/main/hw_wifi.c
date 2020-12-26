#include "hw_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mostly for waiting until IP assignment is complete
 */
static EventGroupHandle_t xWifiEventGroup = NULL;
/*
 * Used for setting the static IP address
 */
#if CONFIG_HW_STATIC_IP_ADDR
  static tcpip_adapter_ip_info_t xIpInfoSta;
#endif
/*
 * xWifiEventGroup gets deleted at the end of vHwSetupWifi(), so we need a flag
 * to let event_callback() know when it's ok to use xEventGroupSetBits()
 */
static BaseType_t xSetupWifiDone = pdFALSE;
/*
 * Function prototypes
 */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void vHwWifiSetup(void) {
  // Setup TCPIP for having a static IP later on, if positioned to do so in menuconfig
  #if CONFIG_HW_STATIC_IP_ADDR
    // Below: copy the menuconfig constant into a mutable string, repeatedly
    // split said string using strtok(), convert the segments into integers with strtoul(),
    // combine those into a single uint32_t, and finally make it a network-safe
    // IPv4 address via lwip_htonl().
    char *pcIpAddr = malloc(strlen(CONFIG_HW_IP_ADDR) + 1);
    strcpy(pcIpAddr, CONFIG_HW_IP_ADDR);
    xIpInfoSta.ip.addr = lwip_htonl(LWIP_MAKEU32(strtoul(strtok(pcIpAddr, "."), NULL, 10),
      strtoul(strtok(NULL, "."), NULL, 10), strtoul(strtok(NULL, "."), NULL, 10),
      strtoul(strtok(NULL, "."), NULL, 10)));
    char *pcNetmaskAddr = malloc(strlen(CONFIG_HW_NETMASK_ADDR) + 1);
    strcpy(pcNetmaskAddr, CONFIG_HW_NETMASK_ADDR);
    xIpInfoSta.netmask.addr = lwip_htonl(LWIP_MAKEU32(strtoul(strtok(pcNetmaskAddr, "."), NULL, 10),
      strtoul(strtok(NULL, "."), NULL, 10), strtoul(strtok(NULL, "."), NULL, 10),
      strtoul(strtok(NULL, "."), NULL, 10)));
    char *pcGwAddr = malloc(strlen(CONFIG_HW_GW_ADDR) + 1);
    strcpy(pcGwAddr, CONFIG_HW_GW_ADDR);
    xIpInfoSta.gw.addr = lwip_htonl(LWIP_MAKEU32(strtoul(strtok(pcGwAddr, "."), NULL, 10),
      strtoul(strtok(NULL, "."), NULL, 10), strtoul(strtok(NULL, "."), NULL, 10),
      strtoul(strtok(NULL, "."), NULL, 10)));
  #endif
  // Basic WiFi init stuff, refer to the Espressif examples for what's going on
  xWifiEventGroup = xEventGroupCreate();
  nvs_flash_init();
  tcpip_adapter_init();
  esp_event_loop_create_default();
  wifi_init_config_t xWifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&xWifiInitConfig);
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
  esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
  wifi_config_t xWifiConfig = {
    .sta = {
      .ssid = CONFIG_HW_WIFI_SSID,
      .password = CONFIG_HW_WIFI_PASSWORD
    }
  };
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(ESP_IF_WIFI_STA, &xWifiConfig);
  // Espressif politely asks us not to use this function in a regular program,
  // but we do it anyways :-)
  esp_wifi_set_protocol(ESP_IF_WIFI_STA, CONFIG_HW_WIFI_PROTOCOL);
  // Set up the MAC address as specified in menuconfig, if applicable
  #if CONFIG_HW_SPECIFY_MAC_ADDR
    // Below: similar to the IP address shenanigans above, except that the
    // MAC has to be copied into a string buffer and then into the proper
    // buffer, since there's no equivalent to LWIP_MAKEU32() or lwip_htonl()
    // for arrays.
    char pcSetMacString[7];
    uint8_t pucSetMac[6];
    char *pcSetMacAddr = malloc(strlen(CONFIG_HW_MAC_ADDR) + 1);
    strcpy(pcSetMacAddr, CONFIG_HW_MAC_ADDR);
    sprintf(pcSetMacString, "%c""%c""%c""%c""%c""%c", (char) strtoul(strtok(pcSetMacAddr, ":"), NULL, 16),
      (char) strtoul(strtok(NULL, ":"), NULL, 16), (char) strtoul(strtok(NULL, ":"), NULL, 16),
      (char) strtoul(strtok(NULL, ":"), NULL, 16), (char) strtoul(strtok(NULL, ":"), NULL, 16),
      (char) strtoul(strtok(NULL, ":"), NULL, 16));
    memcpy(pucSetMac, (void *) pcSetMacString, 6);
    esp_wifi_set_mac(ESP_IF_WIFI_STA, pucSetMac);
  #endif
  esp_wifi_start();
  uint8_t pucGetMac[6];
  esp_wifi_get_mac(ESP_IF_WIFI_STA, pucGetMac);
  ESP_LOGD(__ESP_FILE__, "pucMac "MACSTR, MAC2STR(pucGetMac));
  EventBits_t xWifiBits;
  #if CONFIG_HW_WIFI_TIMEOUT != -1
    xWifiBits = xEventGroupWaitBits(xWifiEventGroup, HW_BIT_WIFI_STA_START | HW_BIT_WIFI_STA_CONNECT |
      HW_BIT_WIFI_IP_GET, pdTRUE, pdTRUE, pdMS_TO_TICKS(1000 * CONFIG_HW_WIFI_TIMEOUT));
  #else
    xWifiBits = xEventGroupWaitBits(xWifiEventGroup, HW_BIT_WIFI_STA_START | HW_BIT_WIFI_STA_CONNECT |
      HW_BIT_WIFI_IP_GET, pdTRUE, pdTRUE, portMAX_DELAY);
  #endif
  // Restart the device upon connection timeout. There's probably a better way
  // to handle this but I'm at a loss right now.
  if (xWifiBits != (HW_BIT_WIFI_STA_START | HW_BIT_WIFI_STA_CONNECT | HW_BIT_WIFI_IP_GET)) {
    esp_restart();
  }
  xSetupWifiDone = pdTRUE;
  // At present, event_handler() remains registered on the off-chance that the
  // device loses WiFi connection for whatever reason so that it may reconnect.
  // This behaviour can be changed in menuconfig
  #if !CONFIG_HW_WIFI_KEEP_EVENT_HANDLER
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler);
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler);
  #endif
  vEventGroupDelete(xWifiEventGroup);
}
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  #if CONFIG_HW_WIFI_RETRY_MAX != -1
    // How many times connection has been attempted
    static uint32_t ulRetryConnectionCounter = 0;
  #endif
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    // Connect once WiFi has fully started
    if (xSetupWifiDone == pdFALSE) {
      xEventGroupSetBits(xWifiEventGroup, HW_BIT_WIFI_STA_START);
    }
    ESP_LOGD(__ESP_FILE__, "STA start");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    if (xSetupWifiDone == pdFALSE) {
      xEventGroupSetBits(xWifiEventGroup, HW_BIT_WIFI_STA_CONNECT);
    }
    ESP_LOGD(__ESP_FILE__, "STA connect");
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(__ESP_FILE__, "STA disconnect");
    #if CONFIG_HW_WIFI_RETRY_MAX != -1
      // Make an attempt to reconnect, but give up if it's futile
      if (ulRetryConnectionCounter < CONFIG_HW_WIFI_RETRY_MAX) {
        esp_wifi_connect();
        ++ulRetryConnectionCounter;
      } else {
        ESP_LOGE(__ESP_FILE__, "STA connect fail, restart");
        esp_restart();
      }
    #else
      esp_wifi_connect();
    #endif
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    #if CONFIG_HW_WIFI_RETRY_MAX != -1
      ulRetryConnectionCounter = 0;
    #endif
    ip_event_got_ip_t* pxEvent = (ip_event_got_ip_t*) event_data;
    ESP_LOGD(__ESP_FILE__, "pxEvent->ip_changed=%d", (int) pxEvent->ip_changed);
    // Only do the thing if IP has changed; prevents the infinite loop of "Oh
    // look, a new IP! Let's do the thing!" from happening.
    if (pxEvent->ip_changed) {
      ESP_LOGD(__ESP_FILE__, "IP get: %s", ip4addr_ntoa(&pxEvent->ip_info.ip));
      // Shutdown DHCPC so that a static IP can be assigned
      #if CONFIG_HW_STATIC_IP_ADDR
        tcpip_adapter_dhcp_status_t xDhcpcStatus;
        tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &xDhcpcStatus);
        if (xDhcpcStatus == TCPIP_ADAPTER_DHCP_STARTED) {
          tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
          tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &xIpInfoSta);
          if (xSetupWifiDone == pdFALSE) {
            xEventGroupSetBits(xWifiEventGroup, HW_BIT_WIFI_IP_GET);
          }
        }
      #else
        if (xSetupWifiDone == pdFALSE) {
          xEventGroupSetBits(xWifiEventGroup, HW_BIT_WIFI_IP_GET);
        }
      #endif
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
    ESP_LOGW(__ESP_FILE__, "IP loss");
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
