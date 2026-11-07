/*
 * Blocking HTTP(S) GET for SD apps and the app catalog. HTTPS is verified against the certificate
 * bundle when it is enabled in the build.
 */
#pragma once

#include <cstddef>
#include <string>

namespace hal::http {

struct Response {
    bool        ok = false;  // false: no HTTP exchange happened, see `error`
    int         status = 0;
    std::string body;
    std::string error;
};

// Reading stops once `max_body` bytes have arrived, so `body` can end up
// slightly longer than that but never much longer. Blocks for up to
// `timeout_ms` per network operation.
Response get(const std::string& url, int timeout_ms, size_t max_body);

struct Download {
    bool        ok = false;   // 200 response written in full, within max_bytes
    int         status = 0;
    size_t      size = 0;     // bytes written
    std::string sha256;       // lower-case hex of the bytes written
    std::string error;        // set when !ok
};

// Stream a GET into `path` (created or truncated), hashing on the way. Fails
// on a non-200 status or once more than `max_bytes` arrive; the partial file
// is left for the caller to remove.
Download download(const std::string& url, const std::string& path,
                  size_t max_bytes, int timeout_ms);

}  // namespace hal::http
