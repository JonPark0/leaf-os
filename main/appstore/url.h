/*
 * URL host extraction and the `net` permission check for http.get.
 * Platform-free.
 */
#pragma once

#include <string>
#include <vector>

namespace appstore {

// Lower-cased host of an http(s) URL: the scheme, any "user@" part, the port
// and everything from the first '/', '?' or '#' are dropped. "" when the URL
// is not http:// or https:// or has no host.
std::string urlHost(const std::string& url);

// True when the URL's host equals one of `net` (already lower case).
bool hostAllowed(const std::vector<std::string>& net, const std::string& url);

}  // namespace appstore
