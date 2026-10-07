#include "updater.hpp"
#include "version.hpp"
#include <curl/curl.h>
#include <openssl/sha.h>
#include <psp2/io/stat.h>
#include <cstdio>
#include <functional>

namespace update {
namespace {
struct Transfer {
    std::atomic<bool>& cancel;
    std::function<void(unsigned)> progress;
    std::string body;
    FILE* file=nullptr;
    std::uint64_t limit=1024*1024, bytes=0;
    SHA256_CTX hash{};
};
std::size_t write_data(char* data,std::size_t size,std::size_t count,void* opaque) {
    auto& t=*static_cast<Transfer*>(opaque);
    if (size && count>t.limit/size) return 0;
    const auto n=size*count;
    if (t.cancel || n>t.limit-t.bytes) return 0;
    if (t.file) {
        if (std::fwrite(data,1,n,t.file)!=n) return 0;
        SHA256_Update(&t.hash,data,n);
    } else t.body.append(data,n);
    t.bytes+=n;
    return n;
}
int progress(void* opaque,curl_off_t total,curl_off_t now,curl_off_t,curl_off_t) {
    auto& t=*static_cast<Transfer*>(opaque);
    if (t.cancel) return 1;
    if (total>0 && static_cast<std::uint64_t>(total)>t.limit) return 1;
    if (total>0) t.progress(static_cast<unsigned>(std::min<curl_off_t>(100,now*100/total)));
    return 0;
}
bool fetch(const std::string& url,Transfer& transfer,long& status) {
    CURL* curl=curl_easy_init(); if (!curl) return false;
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"TG2Vita/" VITA_TG_VERSION);
    curl_easy_setopt(curl,CURLOPT_CAINFO,"app0:assets/certs/cacert.pem");
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(curl,CURLOPT_MAXREDIRS,5L);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,20L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,transfer.file ? 600L : 45L);
    curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1024L);
    curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30L);
    curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl,CURLOPT_FAILONERROR,1L);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_data);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&transfer);
    curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progress);
    curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&transfer);
    const auto result=curl_easy_perform(curl);
    curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
    curl_easy_cleanup(curl);
    return result==CURLE_OK && status==200;
}
}
Updater::Updater() { initialized_=curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK; }
Updater::~Updater() { cancel(); if (initialized_) curl_global_cleanup(); }
void Updater::cancel() { cancel_=true; if (worker_.joinable()) worker_.join(); }
State Updater::state() const { std::lock_guard<std::mutex> lock(mutex_); return state_; }
void Updater::finish(const std::string& status,const std::string& detail,bool available) {
    std::lock_guard<std::mutex> lock(mutex_); state_.status=status; state_.detail=detail; state_.busy=false; state_.available=available;
}
void Updater::check() { run(false); }
void Updater::download() { if (state().available) run(true); }
void Updater::run(bool downloading) {
    if (state().busy) return;
    if (worker_.joinable()) worker_.join();
    if (!initialized_) { finish("Updater initialization failed"); return; }
    cancel_=false;
    { std::lock_guard<std::mutex> lock(mutex_); state_.busy=true; state_.percent=0; state_.status=downloading ? "Downloading update..." : "Checking GitHub Releases..."; state_.detail.clear(); }
    worker_=std::thread([this,downloading] {
        Transfer t{cancel_,[this](unsigned p) { std::lock_guard<std::mutex> lock(mutex_); state_.percent=p; },{},nullptr,1024*1024,0,{}};
        long status=0;
        if (!downloading) {
            if (!fetch("https://api.github.com/repos/"+std::string(repository)+"/releases/latest",t,status)) {
                finish(cancel_ ? "Update check canceled" : status==404 ? "No public release available" : "Update check failed",status ? "HTTP "+std::to_string(status) : "Check Wi-Fi, date/time and retry"); return;
            }
            Release r; std::string error;
            if (!parse_release(t.body,VITA_TG_VERSION,r,error)) { finish(error); return; }
            release_=std::move(r); finish("Version "+release_.version+" available","Cross: download VPK",true); return;
        }
        sceIoMkdir("ux0:download",0777);
        const auto path="ux0:download/TG2Vita-"+release_.version+".vpk";
        const auto partial=path+".part";
        t.limit=release_.size; t.file=std::fopen(partial.c_str(),"wb");
        if (!t.file) { finish("Cannot write download directory",{},true); return; }
        SHA256_Init(&t.hash);
        bool ok=fetch(release_.url,t,status);
        if (std::fclose(t.file)!=0) ok=false;
        unsigned char digest[SHA256_DIGEST_LENGTH]; SHA256_Final(digest,&t.hash);
        std::string hex; const char* digits="0123456789abcdef";
        for (auto b:digest) {hex+=digits[b>>4];hex+=digits[b&15];}
        ok=ok && !cancel_ && t.bytes==release_.size && hex==release_.digest.substr(7);
        if (!ok) { std::remove(partial.c_str()); finish(cancel_ ? "Download canceled" : "Download failed verification","Cross: retry download",true); return; }
        if (std::rename(partial.c_str(),path.c_str())!=0) { std::remove(partial.c_str()); finish("Cannot save verified VPK",{},true); return; }
        finish("Update downloaded",path+"\nExit with Start, then install in VitaShell.");
    });
}
}
