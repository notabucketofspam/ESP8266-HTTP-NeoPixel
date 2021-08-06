#include "hw_http.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Main files used for GET base path
 */
static FILE *pxIndexHtml = NULL;
static FILE *pxFaviconIco = NULL;
/*
 * Keep track of what pixels are assigned to what patterns
 */
static FILE *pxPersistentStateJson = NULL;
/*
 * Used for parsing JSON responses
 */
static jsmn_parser xJsmnParser;
/*
 * Function prototypes
 */
static esp_err_t xFaviconIcoGetHandler(httpd_req_t *req);
static esp_err_t xIndexHtmlGetHandler(httpd_req_t *req);
static esp_err_t xBasePathGetHandler(httpd_req_t *req);
static esp_err_t xFormSubmissionPostHandler(httpd_req_t *req);

void vHwHttpSetup(void) {
  // Setup SPIFFS and open files
  esp_vfs_spiffs_conf_t xSpiffsConfig = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 3,
    .format_if_mount_failed = true
  };
  esp_vfs_spiffs_register(&xSpiffsConfig);
  pxIndexHtml = fopen("/spiffs/index.html", "r");
  pxFaviconIco = fopen("/spiffs/favicon.ico", "rb");
  pxPersistentStateJson = fopen("/spiffs/persistent-state.json", "r+");
  // Setup HTTPD and register event handlers
  static httpd_handle_t xHttpdHandle = NULL;
  httpd_config_t xHttpdConfig = HTTPD_DEFAULT_CONFIG();
  httpd_start(&xHttpdHandle, &xHttpdConfig);
  httpd_uri_t xIndexHtmlGetUri = {
    .uri = "/index.html",
    .method = HTTP_GET,
    .handler = xIndexHtmlGetHandler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(xHttpdHandle, &xIndexHtmlGetUri);
  httpd_uri_t xBasePathGetUri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = xBasePathGetHandler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(xHttpdHandle, &xBasePathGetUri);
  httpd_uri_t xFaviconIcoGetUri = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = xFaviconIcoGetHandler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(xHttpdHandle, &xFaviconIcoGetUri);
  httpd_uri_t xFormSubmissionPostUri = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = xFormSubmissionPostHandler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(xHttpdHandle, &xFormSubmissionPostUri);
  // TODO: setup a GET handler for
  // loading persistent-state.json (or maybe do this automatically instead).

  // Miscellaneous setup functions
  jsmn_init(&xJsmnParser);
}
static esp_err_t xFaviconIcoGetHandler(httpd_req_t *req) {
  // Mostly copy-pasted from xBasePathGetHandler() below
  static uint32_t ulRespChunkCount;
  static BaseType_t xRespChunkCountDone = pdFALSE;
  if (xRespChunkCountDone != pdTRUE) {
    fseek(pxFaviconIco, 0L, SEEK_END);
    int32_t lFaviconIcoSize = ftell(pxFaviconIco);
    rewind(pxFaviconIco);
    ulRespChunkCount = (((lFaviconIcoSize) + (CONFIG_HW_FAVICON_ICO_GET_RESP_BUF_SIZE - 1)) /
      (CONFIG_HW_FAVICON_ICO_GET_RESP_BUF_SIZE)) + 1;
    xRespChunkCountDone = pdTRUE;
  }
  char *pcFaviconIcoGetRespBuf = malloc(CONFIG_HW_FAVICON_ICO_GET_RESP_BUF_SIZE);
  httpd_resp_set_type(req, "image/x-icon");
  for (uint32_t ulRespChunkIndex = 0; ulRespChunkIndex < ulRespChunkCount; ++ulRespChunkIndex) {
    size_t xRespLength = fread(pcFaviconIcoGetRespBuf, 1, CONFIG_HW_FAVICON_ICO_GET_RESP_BUF_SIZE, pxFaviconIco);
    httpd_resp_send_chunk(req, pcFaviconIcoGetRespBuf, (ssize_t) xRespLength);
  }
  httpd_resp_send_chunk(req, NULL, 0);
  rewind(pxFaviconIco);
  free(pcFaviconIcoGetRespBuf);
  return ESP_OK;
}
static esp_err_t xIndexHtmlGetHandler(httpd_req_t *req) {
  httpd_resp_set_status(req, "307 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}
static esp_err_t xBasePathGetHandler(httpd_req_t *req) {
  static uint32_t ulRespChunkCount;
  static BaseType_t xRespChunkCountDone = pdFALSE;
  if (xRespChunkCountDone != pdTRUE) {
    // Do a Yandex search for "c stdio file size" to get what's going on down below
    fseek(pxIndexHtml, 0L, SEEK_END);
    int32_t lIndexHtmlSize = ftell(pxIndexHtml);
    rewind(pxIndexHtml);
    // I still don't know why it's +1 at the end of this.
    ulRespChunkCount = (((lIndexHtmlSize) + (CONFIG_HW_INDEX_HTML_GET_RESP_BUF_SIZE - 1)) /
      (CONFIG_HW_INDEX_HTML_GET_RESP_BUF_SIZE)) + 1;
    xRespChunkCountDone = pdTRUE;
  }
  char *pcIndexHtmlGetRespBuf = malloc(CONFIG_HW_INDEX_HTML_GET_RESP_BUF_SIZE);
  for (uint32_t ulRespChunkIndex = 0; ulRespChunkIndex < ulRespChunkCount; ++ulRespChunkIndex) {
    size_t xRespLength = fread(pcIndexHtmlGetRespBuf, 1, CONFIG_HW_INDEX_HTML_GET_RESP_BUF_SIZE, pxIndexHtml);
    httpd_resp_send_chunk(req, pcIndexHtmlGetRespBuf, (ssize_t) xRespLength);
    printf("%.*s\n", xRespLength, pcIndexHtmlGetRespBuf);
  }
  httpd_resp_send_chunk(req, NULL, 0);
  rewind(pxIndexHtml);
  free(pcIndexHtmlGetRespBuf);
  return ESP_OK;
}
static esp_err_t xFormSubmissionPostHandler(httpd_req_t *req) {
  ESP_LOGI(__ESP_FILE__, "%d", req->content_len);
  char *pcReveiveBuffer = malloc(CONFIG_HW_HTTPD_REQ_RECV_BUF_SIZE);
  int sReceiveSize = httpd_req_recv(req, pcReveiveBuffer, CONFIG_HW_HTTPD_REQ_RECV_BUF_SIZE);
  ESP_LOGI(__ESP_FILE__, "%s", pcReveiveBuffer);
  httpd_resp_send(req, NULL, 0);
  free(pcReveiveBuffer);
  return ESP_OK;
}

#ifdef __cplusplus
}
#endif
