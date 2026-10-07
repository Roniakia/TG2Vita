#include "telegram/auth.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const char* path = argv[1];
    telegram::Credentials credentials;
    std::string error;
    auto check = [&](const std::string& contents, bool expected, const char* name) {
        { std::ofstream out(path); out << contents; }
        const bool loaded = telegram::load_credentials(path, credentials, error);
        if (loaded != expected) { std::cerr << "FAILED: " << name << '\n'; return false; }
        return true;
    };
    const std::string hash(32, 'a');
    const std::string good = "api_id=12345\napi_hash=" + hash + "\nuse_test_dc=true\n";
    if (!check(good, true, "valid credentials") || credentials.api_id != 12345 || !credentials.test_dc) return 1;
    if (!check("api_id=9999999999999999999999999\napi_hash=" + hash, false, "overflow")) return 1;
    if (!check("api_id=0\napi_hash=" + hash, false, "placeholder")) return 1;
    if (!check("api_id=12\napi_id=13\napi_hash=" + hash, false, "duplicate ID")) return 1;
    if (!check("api_id=123\napi_hash=" + hash + "\napi_hash=" + hash, false, "duplicate hash")) return 1;
    if (!check("api_id=123\napi_hash=xyz\n", false, "invalid hash")) return 1;
    if (!check(good + "use_test_dc=maybe\n", false, "invalid server flag")) return 1;
    if (!check("api_id=123\n", false, "missing hash")) return 1;
    if (!check(good + "unexpected=true\n", false, "unknown option")) return 1;
    std::remove(path);
    if (telegram::load_credentials(path, credentials, error)) return 1;
    std::cout << "Credential parsing checks passed\n";
}
