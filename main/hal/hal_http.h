/*
 * Blocking HTTP(S) GET for SD apps. HTTPS is verified against the certificate
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

}  // namespace hal::http
