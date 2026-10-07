#include "update/release.hpp"
#include <cassert>
#include <iostream>
int main() {
    std::array<unsigned,3> v{};
    assert(update::version("v0.10.0",v)); assert(!update::version("0.1.0-beta",v));
    assert(!update::version("0.01.0",v)); assert(!update::version("99999999.0.0",v));
    const std::string digest(64,'a');
    std::string body="{\"tag_name\":\"v0.10.0\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"vita_tg.vpk\",\"state\":\"uploaded\",\"size\":100,\"digest\":\"sha256:"+digest+"\",\"browser_download_url\":\"https://github.com/Roniakia/TG2Vita/releases/download/v0.10.0/vita_tg.vpk\"}]}";
    update::Release r; std::string error;
    assert(update::parse_release(body,"0.6.0",r,error)); assert(r.size==100);
    assert(!update::parse_release(body,"0.10.0",r,error));
    auto bad=body;bad.replace(bad.find("sha256:"),7,"sha512:"); assert(!update::parse_release(bad,"0.6.0",r,error));
    bad=body;bad.replace(bad.find("Roniakia/TG2Vita/releases"),8,"attacker");assert(!update::parse_release(bad,"0.6.0",r,error));
    bad=body;bad.replace(bad.find("\"prerelease\":false"),18,"\"prerelease\":true");assert(!update::parse_release(bad,"0.6.0",r,error));
    bad=body;bad.replace(bad.find("\"size\":100"),10,"\"size\":999999999");assert(!update::parse_release(bad,"0.6.0",r,error));
    assert(!update::parse_release("{}","0.6.0",r,error));
    std::cout<<"Release validation tests passed\n";
}
