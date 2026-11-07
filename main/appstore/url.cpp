#include "url.h"

#include <cctype>

namespace appstore {

std::string urlHost(const std::string& url)
{
    size_t p;
    if (url.compare(0, 7, "http://") == 0)       p = 7;
    else if (url.compare(0, 8, "https://") == 0) p = 8;
    else return "";

    size_t end = url.find_first_of("/?#", p);
    if (end == std::string::npos) end = url.size();
    std::string auth = url.substr(p, end - p);

    const size_t at = auth.rfind('@');
    if (at != std::string::npos) auth = auth.substr(at + 1);
    const size_t colon = auth.find(':');
    if (colon != std::string::npos) auth = auth.substr(0, colon);

    for (char& c : auth) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return auth;
}

bool hostAllowed(const std::vector<std::string>& net, const std::string& url)
{
    const std::string host = urlHost(url);
    if (host.empty()) return false;
    for (const std::string& h : net)
        if (h == host) return true;
    return false;
}

}  // namespace appstore
