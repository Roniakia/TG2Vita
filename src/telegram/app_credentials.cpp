#include "auth.hpp"
#include "application_credentials.hpp"

namespace telegram {
bool load_application_credentials(Credentials& result, std::string& error) {
    if constexpr (build_credentials::enabled) {
        result.api_id = build_credentials::api_id;
        result.api_hash = build_credentials::api_hash;
        result.test_dc = false;
        error.clear();
        return true;
    } else {
        return load_credentials("ux0:data/vita-tg/telegram.conf", result, error);
    }
}
}
