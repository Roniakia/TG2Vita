#include "telegram/auth.hpp"
#include "state_checks.hpp"
#include <vita2d.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <memory>
#include <cstdlib>
#include <string>
extern "C" { unsigned int _newlib_heap_size_user = 128 * 1024 * 1024; }
extern "C" const unsigned int sceUserMainThreadStackSize = 512 * 1024;
void checkpoint(const std::string& value) {
    int fd = sceIoOpen("ux0:data/vita-tg-auth-tests/result.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0600);
    if (fd >= 0) { std::string line = value + "\n"; sceIoWrite(fd, line.data(), line.size()); sceIoClose(fd); }
}
namespace telegram { void auth_test_log(int, const char* message) { checkpoint(message); } }
int main() {
    vita2d_init(); auto* font = vita2d_load_default_pgf();
    sceIoMkdir("ux0:data/vita-tg-auth-tests", 0700);
    checkpoint("Starting isolated engine initialization test");
    const bool states_ok=telegram::AuthTestAccess::run();
    checkpoint(states_ok ? "PASS: synthetic phone/code/2FA/Ready/profile states" : "FAIL: synthetic authorization states");
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    void* memory = std::malloc(4*1024*1024);
    SceNetInitParam param{}; param.memory=memory; param.size=4*1024*1024;
    bool network = sceNetInit(&param) == 0 && sceNetCtlInit() == 0;
    // Deliberately fictitious app credentials. No phone/code request is sent.
    telegram::Credentials credentials; credentials.api_id=12345;
    credentials.api_hash=std::string(32, 'a'); credentials.test_dc=true;
    auto auth=std::make_unique<telegram::Auth>("ux0:data/vita-tg-auth-tests");
    checkpoint(network ? "Network ready" : "Network failed");
    if (network && states_ok) auth->start(credentials);
    std::string last, result="Waiting for native TDLib";
    int passes=0;
    while (true) {
        if (auth) {
            auth->poll();
            if (last != auth->status()) { last=auth->status(); checkpoint(last); }
            if (auth->stage()==telegram::AuthStage::Phone) {
                ++passes; checkpoint(passes==1 ? "PASS: fresh encrypted session initialization" : "PASS: encrypted session reopened");
                auth->close(); auth.reset(); checkpoint("PASS: client shutdown");
                if (passes==1) { auth=std::make_unique<telegram::Auth>("ux0:data/vita-tg-auth-tests"); auth->start(credentials); last.clear(); }
                else result="PASS: engine startup, session reopen and shutdown";
            } else if (auth->stage()==telegram::AuthStage::Failed) result=auth->status();
            else result=auth->status();
        }
        vita2d_start_drawing(); vita2d_clear_screen();
        vita2d_pgf_draw_text(font,30,70,RGBA8(255,255,255,255),1.2f,"Native TDLib initialization test");
        vita2d_pgf_draw_text(font,30,150,RGBA8(80,210,160,255),0.85f,result.c_str());
        vita2d_pgf_draw_text(font,30,230,RGBA8(180,190,200,255),0.8f,"Isolated test storage. No account login attempted.");
        vita2d_end_drawing(); vita2d_swap_buffers(); sceDisplayWaitVblankStart();
    }
}
