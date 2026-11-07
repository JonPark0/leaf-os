#include "hal_http.h"

#include <esp_err.h>
#include <esp_http_client.h>
#include <sdkconfig.h>
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
#include <esp_crt_bundle.h>
#endif

namespace hal::http {

Response get(const std::string& url, int timeout_ms, size_t max_body)
{
    Response r;

    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.timeout_ms = timeout_ms;
    cfg.user_agent = "M5PaperMono-Lite";
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        r.error = "init failed";
        return r;
    }

    const esp_err_t e = esp_http_client_open(c, 0);
    if (e != ESP_OK) {
        esp_http_client_cleanup(c);
        r.error = esp_err_to_name(e);
        return r;
    }
    esp_http_client_fetch_headers(c);
    r.status = esp_http_client_get_status_code(c);

    char buf[512];
    for (;;) {
        const int n = esp_http_client_read(c, buf, sizeof(buf));
        if (n <= 0) break;
        r.body.append(buf, n);
        if (r.body.size() >= max_body) break;
    }
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    r.ok = true;
    return r;
}

}  // namespace hal::http
