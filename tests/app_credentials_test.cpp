#include "telegram/auth.hpp"
#include <iostream>
int main(int argc, char** argv) {
    if (argc!=2) return 2;
    telegram::Credentials built, source;
    std::string error;
    if (!telegram::load_application_credentials(built, error)) return 1;
    if (!telegram::load_credentials(argv[1], source, error)) return 1;
    if (built.api_id!=source.api_id || built.api_hash!=source.api_hash || built.test_dc) return 1;
    std::cout << "Built-in credentials match private build inputs and select production; no device config required\n";
}
