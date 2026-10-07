#pragma once
#include <jansson.h>
#include <array>
#include <cstdint>
#include <cstdlib>
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
enum class Channel { Stable, Beta };
struct Version {
    std::array<unsigned,3> core{};
    unsigned beta=0;
    bool prerelease=false;
    bool operator<(const Version& other) const {
        if (core!=other.core) return core<other.core;
        if (prerelease!=other.prerelease) return prerelease;
        return beta<other.beta;
    }
};
inline bool release_version(const std::string& text,Version& out) {
    const auto suffix=text.find("-beta.");
    if (suffix==std::string::npos) return version(text,out.core);
    if (!version(text.substr(0,suffix),out.core)) return false;
    const auto number=text.substr(suffix+6);
    if (number.empty() || number[0]=='0' || number.size()>4) return false;
    for (char c:number) { if(c<'0' || c>'9') return false; out.beta=out.beta*10+unsigned(c-'0'); }
    out.prerelease=true; return true;
}
inline std::string field(json_t* object,const char* name) {
    const char* value=json_string_value(json_object_get(object,name));
    return value ? value : "";
}
inline bool parse_release(const std::string& body,const std::string& current,Release& release,std::string& error,Channel channel=Channel::Stable) {
    json_error_t e{}; json_t* root=json_loadb(body.data(),body.size(),JSON_REJECT_DUPLICATES,&e);
    if (!root) { error="Invalid release response"; return false; }
    struct Guard { json_t* p; ~Guard(){json_decref(p);} } guard{root};
    Version latest{},installed{};
    const auto tag=field(root,"tag_name");
    if (!json_is_object(root) || !json_is_false(json_object_get(root,"draft")) ||
        !(json_is_false(json_object_get(root,"prerelease")) || (channel==Channel::Beta && json_is_true(json_object_get(root,"prerelease")))) || !release_version(tag,latest) || !release_version(current,installed) ||
        latest.prerelease!=json_is_true(json_object_get(root,"prerelease"))) {
        error="Unsupported release metadata"; return false;
    }
    if (!(installed<latest)) { error="You have the latest version"; return false; }
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
inline bool parse_releases(const std::string& body,const std::string& current,Release& release,std::string& error) {
    json_error_t e{}; json_t* root=json_loadb(body.data(),body.size(),JSON_REJECT_DUPLICATES,&e);
    if (!json_is_array(root)) { if(root) json_decref(root); error="Invalid release list"; return false; }
    bool found=false; Version best{}; std::size_t i; json_t* item;
    json_array_foreach(root,i,item) {
        char* text=json_dumps(item,JSON_COMPACT); if(!text) continue;
        Release candidate; std::string ignored;
        const bool valid=parse_release(text,current,candidate,ignored,Channel::Beta); free(text);
        Version v{};
        if(valid && release_version(candidate.version,v) && (!found || best<v)) { release=std::move(candidate);best=v;found=true; }
    }
    json_decref(root); if(!found) error="No newer compatible release available"; return found;
}

}
