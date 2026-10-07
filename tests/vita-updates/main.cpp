#include "update/updater.hpp"
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <curl/curl.h>
#include <cstdio>
#include <cstdlib>
extern "C" { unsigned int _newlib_heap_size_user=32*1024*1024; }
int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    void* memory=std::malloc(4*1024*1024);
    SceNetInitParam p{}; p.memory=memory;p.size=4*1024*1024;
    const int net=sceNetInit(&p),ctl=sceNetCtlInit();
    sceIoMkdir("ux0:data/vita-tg-update-tests",0777);
    auto* file=std::fopen("ux0:data/vita-tg-update-tests/result.txt","w");
    if (file) {std::fprintf(file,"Net %d Ctl %d\n",net,ctl);std::fflush(file);}
    {
        update::Updater updater; updater.check();
        while (updater.state().busy) sceKernelDelayThread(100000);
        const auto result=updater.state();
        if (file) std::fprintf(file,"%s\n%s\n%s\n",curl_version(),result.status.c_str(),result.detail.c_str());
    }
    if (file) std::fclose(file);
    sceNetCtlTerm();sceNetTerm();std::free(memory);
    sceKernelExitProcess(0);return 0;
}
