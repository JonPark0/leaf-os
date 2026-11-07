#include "kv.h"

namespace core::kv {

std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && static_cast<unsigned char>(s[a]) <= ' ') ++a;
    while (b > a && static_cast<unsigned char>(s[b - 1]) <= ' ') --b;
    return s.substr(a, b - a);
}

bool split(const std::string& line, std::string& key, std::string& val)
{
    const size_t eq = line.find('=');
    if (eq == std::string::npos) return false;
    key = trim(line.substr(0, eq));
    val = trim(line.substr(eq + 1));
    return !key.empty();
}

}  // namespace core::kv
