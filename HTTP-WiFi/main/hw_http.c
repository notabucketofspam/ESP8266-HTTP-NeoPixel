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
 * Sizes of each of the files above
 */
static int32_t lIndexHtmlSize = 0;
static int32_t lFaviconIcoSize = 0;
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
static esp_err_t xFaviconIcoGetHandler(httpd_req_t *request);
static esp_err_t xIndexHtmlGetHandler(httpd_req_t *request);
static esp_err_t xBasePathGetHandler(httpd_req_t *request);

void vHwHttpSetup(void) {
  // Setup SPIFFS, open files, and get file lengths
  esp_vfs_spiffs_conf_t xSpiffsConfig = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 3,
    .format_if_mount_failed = true
  };
  esp_vfs_spiffs_register(&xSpiffsConfig);
  pxIndexHtml = fopen("/spiffs/index.html", "r");
  // Do a Yandex search for "c stdio file size" to get what's going on down below
  fseek(pxIndexHtml, 0L, SEEK_END);
  lIndexHtmlSize = ftell(pxIndexHtml);
  rewind(pxIndexHtml);
  pxFaviconIco = fopen("/spiffs/favicon.ico", "rb");
  fseek(pxFaviconIco, 0L, SEEK_END);
  lFaviconIcoSize = ftell(pxFaviconIco);
  rewind(pxFaviconIco);
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
  // TODO: setup a POST handler for sending pattern data, and maybe a GET handler for
  // loading persistent-state.json (or maybe do this automatically instead).

  // Miscellaneous setup functions
  jsmn_init(&xJsmnParser);
}
static esp_err_t xFaviconIcoGetHandler(httpd_req_t *req) {
  // Mostly copy-pasted from xBasePathGetHandler() below.
  char *pcFaviconIcoGetRespBuf = malloc(CONFIG_HW_FAVICON_ICO_GET_RESP_BUF_SIZE);
  httpd_resp_set_type(req, "image/x-icon");
  uint32_t ulRespChunkCount = (((lFaviconIcoSize) + (CONFIG_HW_FAVICON_ICO_GET_RESP_BUF_SIZE - 1)) /
    (CONFIG_HW_FAVICON_ICO_GET_RESP_BUF_SIZE)) + 1;
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
  char *pcIndexHtmlGetRespBuf = malloc(CONFIG_HW_INDEX_HTML_GET_RESP_BUF_SIZE);
  // I still don't know why it's +1 at the end of this.
  uint32_t ulRespChunkCount = (((lIndexHtmlSize) + (CONFIG_HW_INDEX_HTML_GET_RESP_BUF_SIZE - 1)) /
    (CONFIG_HW_INDEX_HTML_GET_RESP_BUF_SIZE)) + 1;
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

#ifdef __cplusplus
}
#endif
