#include "auth.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <cerrno>
namespace telegram {
namespace {
void wipe(std::string& s) {
    volatile char* data = s.empty() ? nullptr : &s[0];
    for (std::size_t i = 0; i < s.size(); ++i) data[i] = 0;
    s.clear();
}
}
bool load_credentials(const char* path, Credentials& result, std::string& error) {
    std::ifstream file(path);
    if (!file) { error = "Place telegram.conf in ux0:data/vita-tg/"; return false; }
    Credentials parsed;
    std::string line;
    bool id_seen = false, hash_seen = false, server_seen = false;
    while (std::getline(file, line)) {
        if (line.size() > 512) { error = "Configuration line is too long"; return false; }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        auto separator = line.find('=');
        if (separator == std::string::npos) { error = "Invalid configuration format"; return false; }
        auto key = line.substr(0, separator), value = line.substr(separator + 1);
        if (key == "api_id") {
            if (id_seen || value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); })) {
                error = "Invalid api_id"; return false;
            }
            id_seen = true;
            char* end = nullptr;
            errno = 0;
            const long id = std::strtol(value.c_str(), &end, 10);
            if (errno == ERANGE || *end || id <= 0 || id > std::numeric_limits<int>::max()) { error = "Invalid api_id"; return false; }
            parsed.api_id = static_cast<int>(id);
        } else if (key == "api_hash") {
            if (hash_seen || value.size() != 32 || !std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c); })) {
                error = "api_hash must have 32 hexadecimal characters"; return false;
            }
            hash_seen = true;
            parsed.api_hash = value;
        } else if (key == "use_test_dc") {
            if (server_seen || (value != "true" && value != "false")) { error = "use_test_dc must be true or false"; return false; }
            server_seen = true;
            parsed.test_dc = value == "true";
        } else { error = "Unknown configuration setting"; return false; }
        wipe(value);
        wipe(line);
    }
    if (!id_seen || !hash_seen) { error = "Configuration requires api_id and api_hash"; return false; }
    result = std::move(parsed);
    error.clear();
    return true;
}

}
