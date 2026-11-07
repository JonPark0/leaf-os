#include "hal_http.h"

#include <cstdio>
#include <memory>

#include <esp_err.h>
#include <esp_http_client.h>
#include <mbedtls/sha256.h>
#include <sdkconfig.h>
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
#include <esp_crt_bundle.h>
#endif

namespace hal::http {
namespace {

esp_http_client_handle_t openClient(const std::string& url, int timeout_ms)
{
    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.timeout_ms = timeout_ms;
    cfg.user_agent = "M5PaperMono-Lite";
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    return esp_http_client_init(&cfg);
}

}  // namespace

Response get(const std::string& url, int timeout_ms, size_t max_body)
{
    Response r;
    esp_http_client_handle_t c = openClient(url, timeout_ms);
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

Download download(const std::string& url, const std::string& path,
                  size_t max_bytes, int timeout_ms)
{
    Download d;
    esp_http_client_handle_t c = openClient(url, timeout_ms);
    if (!c) { d.error = "init failed"; return d; }

    const esp_err_t e = esp_http_client_open(c, 0);
    if (e != ESP_OK) {
        esp_http_client_cleanup(c);
        d.error = esp_err_to_name(e);
        return d;
    }
    esp_http_client_fetch_headers(c);
    d.status = esp_http_client_get_status_code(c);
    if (d.status != 200) {
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        d.error = "HTTP " + std::to_string(d.status);
        return d;
    }

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        d.error = "cannot write";
        return d;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    constexpr size_t kChunk = 4096;
    std::unique_ptr<char[]> buf(new char[kChunk]);
    bool failed = false;
    for (;;) {
        const int n = esp_http_client_read(c, buf.get(), kChunk);
        if (n < 0) { d.error = "read failed"; failed = true; break; }
        if (n == 0) break;
        d.size += static_cast<size_t>(n);
        if (d.size > max_bytes) { d.error = "too large"; failed = true; break; }
        if (std::fwrite(buf.get(), 1, static_cast<size_t>(n), f) != static_cast<size_t>(n)) {
            d.error = "cannot write"; failed = true; break;
        }
        mbedtls_sha256_update(&sha, reinterpret_cast<const unsigned char*>(buf.get()),
                              static_cast<size_t>(n));
    }
    if (!esp_http_client_is_complete_data_received(c) && !failed) {
        d.error = "connection closed";
        failed = true;
    }
    if (std::fclose(f) != 0 && !failed) { d.error = "cannot write"; failed = true; }
    esp_http_client_close(c);
    esp_http_client_cleanup(c);

    unsigned char digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);
    static const char kHex[] = "0123456789abcdef";
    d.sha256.reserve(64);
    for (unsigned char b : digest) {
        d.sha256.push_back(kHex[b >> 4]);
        d.sha256.push_back(kHex[b & 15]);
    }
    d.ok = !failed;
    return d;
}

}  // namespace hal::http
