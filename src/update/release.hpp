#pragma once
#include <jansson.h>
#include <array>
#include <cstdint>
#include <string>

namespace update {
constexpr const char* repository = "Roniakia/TG2Vita";
constexpr std::uint64_t max_package = 64 * 1024 * 1024;
struct Release { std::string version, url, digest; std::uint64_t size = 0; };
inline bool version(const std::string& text, std::array<unsigned,3>& out) {
    std::size_t pos = !text.empty() && text[0]=='v' ? 1 : 0;
    for (unsigned i=0;i<3;++i) {
        unsigned value=0; const auto start=pos;
        while (pos<text.size() && text[pos]>='0' && text[pos]<='9') {
            value=value*10+static_cast<unsigned>(text[pos++]-'0');
            if (value>9999) return false;
        }
        if (pos==start || (pos-start>1 && text[start]=='0')) return false;
        out[i]=value;
        if (i<2 && (pos==text.size() || text[pos++]!='.')) return false;
    }
    return pos==text.size();
}
inline std::string field(json_t* object,const char* name) {
    const char* value=json_string_value(json_object_get(object,name));
    return value ? value : "";
}
inline bool parse_release(const std::string& body,const std::string& current,Release& release,std::string& error) {
    json_error_t e{}; json_t* root=json_loadb(body.data(),body.size(),JSON_REJECT_DUPLICATES,&e);
    if (!root) { error="Invalid release response"; return false; }
    struct Guard { json_t* p; ~Guard(){json_decref(p);} } guard{root};
    std::array<unsigned,3> latest{},installed{};
    const auto tag=field(root,"tag_name");
    if (!json_is_object(root) || !json_is_false(json_object_get(root,"draft")) ||
        !json_is_false(json_object_get(root,"prerelease")) || !version(tag,latest) || !version(current,installed)) {
        error="Unsupported release metadata"; return false;
    }
    if (latest<=installed) { error="You have the latest version"; return false; }
    json_t* assets=json_object_get(root,"assets");
    if (!json_is_array(assets)) { error="Release has no VPK"; return false; }
    const std::string prefix="https://github.com/"+std::string(repository)+"/releases/download/"+tag+"/";
    std::size_t i; json_t* asset;
    json_array_foreach(assets,i,asset) {
        if (field(asset,"name")!="vita_tg.vpk" || field(asset,"state")!="uploaded") continue;
        Release candidate{tag,field(asset,"browser_download_url"),field(asset,"digest"),0};
        auto size=json_object_get(asset,"size");
        if (!json_is_integer(size) || json_integer_value(size)<=0 || json_integer_value(size)>static_cast<json_int_t>(max_package) ||
            candidate.url!=prefix+"vita_tg.vpk" || candidate.digest.size()!=71 || candidate.digest.substr(0,7)!="sha256:") break;
        bool hex=true;
        for (std::size_t n=7;n<candidate.digest.size();++n) if (!((candidate.digest[n]>='0' && candidate.digest[n]<='9') || (candidate.digest[n]>='a' && candidate.digest[n]<='f'))) hex=false;
        if (!hex) break;
        candidate.size=static_cast<std::uint64_t>(json_integer_value(size)); release=std::move(candidate); return true;
    }
    error="Release needs a valid VPK with SHA-256 digest"; return false;
}
}
