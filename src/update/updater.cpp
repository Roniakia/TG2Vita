#include "updater.hpp"
#include "version.hpp"
#include <curl/curl.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <psp2/kernel/rng.h>
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
bool fetch(const std::string& url,Transfer& transfer,long& status,std::string& error) {
    CURL* curl=curl_easy_init(); if (!curl) { error="curl initialization failed"; return false; }
    // Vita has IPv4 sockets and the installed OpenSSL port has no Unix CA paths.
    FILE* ca=std::fopen("app0:assets/certs/cacert.pem","rb");
    std::string certificates;
    if (ca) {
        char buffer[4096]; std::size_t n;
        while ((n=std::fread(buffer,1,sizeof(buffer),ca))!=0 && certificates.size()+n<=1024*1024)
            certificates.append(buffer,n);
        if (std::ferror(ca) || !std::feof(ca)) certificates.clear();
        std::fclose(ca);
    }
    if (certificates.empty()) { error="Packaged CA bundle cannot be read; reinstall VPK"; curl_easy_cleanup(curl); return false; }
    curl_blob bundle{const_cast<char*>(certificates.data()),certificates.size(),CURL_BLOB_NOCOPY};
    CURLcode options=CURLE_OK;
    auto option=[&](CURLoption name,auto value) {
        if (options==CURLE_OK) options=curl_easy_setopt(curl,name,value);
    };
    option(CURLOPT_CAINFO_BLOB,&bundle);
    option(CURLOPT_IPRESOLVE,static_cast<long>(CURL_IPRESOLVE_V4));
    option(CURLOPT_HTTP_VERSION,static_cast<long>(CURL_HTTP_VERSION_1_1));
    option(CURLOPT_URL,url.c_str());
    option(CURLOPT_USERAGENT,"TG2Vita/" VITA_TG_VERSION);
    option(CURLOPT_CAINFO,static_cast<const char*>(nullptr));
    option(CURLOPT_CAPATH,static_cast<const char*>(nullptr));
    option(CURLOPT_SSL_VERIFYPEER,1L);
    option(CURLOPT_SSL_VERIFYHOST,2L);
    option(CURLOPT_PROTOCOLS_STR,"https");
    option(CURLOPT_REDIR_PROTOCOLS_STR,"https");
    option(CURLOPT_FOLLOWLOCATION,1L);
    option(CURLOPT_MAXREDIRS,5L);
    option(CURLOPT_CONNECTTIMEOUT,20L);
    option(CURLOPT_TIMEOUT,transfer.file ? 600L : 45L);
    option(CURLOPT_LOW_SPEED_LIMIT,1024L);
    option(CURLOPT_LOW_SPEED_TIME,30L);
    option(CURLOPT_NOSIGNAL,1L);
    option(CURLOPT_FAILONERROR,1L);
    option(CURLOPT_WRITEFUNCTION,write_data);
    option(CURLOPT_WRITEDATA,&transfer);
    option(CURLOPT_NOPROGRESS,0L);
    option(CURLOPT_XFERINFOFUNCTION,progress);
    option(CURLOPT_XFERINFODATA,&transfer);
    const auto result=options==CURLE_OK ? curl_easy_perform(curl) : options;
    curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
    if (result!=CURLE_OK) error="curl "+std::to_string(result)+": "+curl_easy_strerror(result);
    curl_easy_cleanup(curl);
    return result==CURLE_OK && status==200;
}
}
Updater::Updater() {
    // Do not depend on TDLib having already seeded OpenSSL, especially before login.
    std::array<unsigned char,64> entropy{};
    if (sceKernelGetRandomNumber(entropy.data(),entropy.size())<0) return;
    RAND_seed(entropy.data(),entropy.size());
    std::fill(entropy.begin(),entropy.end(),0);
    initialized_=RAND_status()==1 && curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK;
}
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
        long status=0; std::string transport_error;
        if (!downloading) {
            if (!fetch("https://api.github.com/repos/"+std::string(repository)+"/releases/latest",t,status,transport_error)) {
                finish(cancel_ ? "Update check canceled" : status==404 ? "No public release available" : "Update check failed",status ? "HTTP "+std::to_string(status) : transport_error); return;
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
        bool ok=fetch(release_.url,t,status,transport_error);
        if (std::fclose(t.file)!=0) ok=false;
        unsigned char digest[SHA256_DIGEST_LENGTH]; SHA256_Final(digest,&t.hash);
        std::string hex; const char* digits="0123456789abcdef";
        for (auto b:digest) {hex+=digits[b>>4];hex+=digits[b&15];}
        ok=ok && !cancel_ && t.bytes==release_.size && hex==release_.digest.substr(7);
        if (!ok) { std::remove(partial.c_str()); finish(cancel_ ? "Download canceled" : "Download failed verification",transport_error.empty() ? "Size or SHA-256 mismatch; Cross: retry" : transport_error,true); return; }
        if (std::rename(partial.c_str(),path.c_str())!=0) { std::remove(partial.c_str()); finish("Cannot save verified VPK",{},true); return; }
        finish("Update downloaded",path+"\nExit with Start, then install in VitaShell.");
    });
}
}
