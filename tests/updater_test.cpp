#include "update/updater.hpp"
#include <curl/curl.h>
#include <openssl/sha.h>
#include <psp2/kernel/rng.h>
#include <psp2/io/stat.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <map>
#include <cstdarg>
#include <cstring>
#undef curl_easy_setopt
#undef curl_easy_getinfo
namespace {
struct Handle { curl_write_callback write=nullptr; void* data=nullptr; curl_xferinfo_callback progress=nullptr; void* progress_data=nullptr; std::string url; long http=0; char* diagnostic=nullptr; };
std::string payload="verified test package",metadata;
CURLcode response=CURLE_OK, option_failure=CURLE_OK;
bool slow=false; unsigned requests=0;
std::map<CURLoption,long> numbers;
std::string hash(const std::string& s) { unsigned char d[32]; SHA256(reinterpret_cast<const unsigned char*>(s.data()),s.size(),d); std::string out; for(auto b:d) {out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];} return out; }
void wait(update::Updater& u) { for(int n=0;n<500 && u.state().busy;++n) std::this_thread::sleep_for(std::chrono::milliseconds(2));assert(!u.state().busy); }
}
extern "C" {
int sceKernelGetRandomNumber(void* data,std::size_t n) {std::memset(data,42,n);return 0;}
int sceIoMkdir(const char* p,int) {std::filesystem::create_directories(p);return 0;}
CURLcode curl_global_init(long) {return CURLE_OK;}
void curl_global_cleanup() {}
CURL* curl_easy_init() {return reinterpret_cast<CURL*>(new Handle);}
void curl_easy_cleanup(CURL* h) {delete reinterpret_cast<Handle*>(h);}
CURLcode curl_easy_setopt(CURL* h,CURLoption name,...) {
    if(option_failure!=CURLE_OK) return option_failure;
    auto& c=*reinterpret_cast<Handle*>(h);va_list v;va_start(v,name);
    switch(name) {
        case CURLOPT_ERRORBUFFER:c.diagnostic=va_arg(v,char*);break;
        case CURLOPT_URL:c.url=va_arg(v,const char*);break;
        case CURLOPT_WRITEFUNCTION:c.write=va_arg(v,curl_write_callback);break;
        case CURLOPT_WRITEDATA:c.data=va_arg(v,void*);break;
        case CURLOPT_XFERINFOFUNCTION:c.progress=va_arg(v,curl_xferinfo_callback);break;
        case CURLOPT_XFERINFODATA:c.progress_data=va_arg(v,void*);break;
        case CURLOPT_CAINFO_BLOB:{auto* blob=va_arg(v,curl_blob*);assert(blob && blob->len>0);break;}
        default:if(name<CURLOPTTYPE_OBJECTPOINT) numbers[name]=va_arg(v,long);break;
    }
    va_end(v);return CURLE_OK;
}
CURLcode curl_easy_getinfo(CURL* h,CURLINFO info,...) {assert(info==CURLINFO_RESPONSE_CODE || info==CURLINFO_OS_ERRNO);va_list v;va_start(v,info);*va_arg(v,long*)=info==CURLINFO_OS_ERRNO ? 111 : reinterpret_cast<Handle*>(h)->http;va_end(v);return CURLE_OK;}
CURLcode curl_easy_perform(CURL* h) {
    ++requests;auto& c=*reinterpret_cast<Handle*>(h);
    if (slow) {for(int i=0;i<100;++i) {if(c.progress(c.progress_data,100,0,0,0)) return CURLE_ABORTED_BY_CALLBACK;std::this_thread::sleep_for(std::chrono::milliseconds(2));}}
    if(response!=CURLE_OK) {std::strcpy(c.diagnostic,"Public test endpoint connection refused");return response;}
    c.http=200;auto body=c.url.find("api.github.com")!=std::string::npos ? metadata : payload;
    if(c.url.find("per_page=30")!=std::string::npos) body="["+body+"]";
    if(c.write(body.data(),1,body.size(),c.data)!=body.size()) return CURLE_WRITE_ERROR;
    return CURLE_OK;
}
const char* curl_easy_strerror(CURLcode) {return "test transport failure";}
}
int main() {
    std::filesystem::create_directories("app0:assets/certs"); std::ofstream("app0:assets/certs/cacert.pem")<<"fixture CA";
    metadata="{\"tag_name\":\"v99.0.0\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"vita_tg.vpk\",\"state\":\"uploaded\",\"size\":"+std::to_string(payload.size())+",\"digest\":\"sha256:"+hash(payload)+"\",\"browser_download_url\":\"https://github.com/Roniakia/TG2Vita/releases/download/v99.0.0/vita_tg.vpk\"}]}";
    update::Updater u;u.check();wait(u);assert(u.state().available);
    assert(numbers[CURLOPT_SSL_VERIFYPEER]==1 && numbers[CURLOPT_SSL_VERIFYHOST]==2);
    assert(numbers[CURLOPT_IPRESOLVE]==CURL_IPRESOLVE_V4 && numbers[CURLOPT_HTTP_VERSION]==CURL_HTTP_VERSION_1_1);
    u.set_channel(update::Channel::Beta);wait(u);assert(u.state().available && u.state().channel==update::Channel::Beta);
    { update::Updater restored; assert(restored.state().channel==update::Channel::Beta); }
    u.set_channel(update::Channel::Stable);wait(u);assert(u.state().available);
    u.download();wait(u);assert(u.state().status=="Update downloaded");
    const std::string path="ux0:download/TG2Vita-v99.0.0.vpk";
    std::ifstream file(path);std::string saved((std::istreambuf_iterator<char>(file)),{});assert(saved==payload);
    u.check();wait(u);payload[0]='X';u.download();wait(u);assert(u.state().available && !std::filesystem::exists(path+".part"));
    response=CURLE_COULDNT_CONNECT;u.check();wait(u);assert(u.state().detail.find("curl 7:")!=std::string::npos && u.state().detail.find("socket 111")!=std::string::npos && u.state().detail.find("connection refused")!=std::string::npos && !u.state().available);
    response=CURLE_OK;slow=true;u.check();u.cancel();assert(u.state().status=="Update check canceled");slow=false;
    option_failure=CURLE_UNKNOWN_OPTION;auto before=requests;u.check();wait(u);assert(requests==before && u.state().detail.find("curl 48:")!=std::string::npos);
    option_failure=CURLE_OK;std::filesystem::remove("app0:assets/certs/cacert.pem");u.check();wait(u);assert(requests==before && u.state().detail.find("CA bundle")!=std::string::npos);
    std::cout<<"Updater transport, verified download, rejection, cancellation and error tests passed\n";
}
