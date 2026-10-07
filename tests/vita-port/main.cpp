#include <vita2d.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/rng.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include <td/utils/port/EventFd.h>
#include <td/utils/port/FileFd.h>
#include <td/utils/port/thread.h>
#include <td/utils/port/thread_local.h>
#include <td/utils/Random.h>
#include <td/utils/port/SocketFd.h>
#include "vita_socketpair.h"
#include <td/utils/crypto.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <openssl/rand.h>
extern "C" { unsigned int _newlib_heap_size_user = 32 * 1024 * 1024; }
int main() {
    sceIoMkdir("ux0:data/vita-tg", 0700);
    int trace = sceIoOpen("ux0:data/vita-tg/port-check.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0600);
    if (trace >= 0) sceIoClose(trace);
    auto step = [&](const char* name) {
        int fd = sceIoOpen("ux0:data/vita-tg/port-check.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0600);
        if (fd >= 0) { sceIoWrite(fd, name, std::strlen(name)); sceIoWrite(fd, "\n", 1); sceIoClose(fd); }
    };
    step("graphics init");
    int graphics = vita2d_init();
    step(graphics < 0 ? "graphics failed" : "graphics ready");
    step("load font");
    auto* font = vita2d_load_default_pgf();
    if (!font) { step("font failed"); vita2d_fini(); sceKernelExitProcess(1); return 1; }
    step("font ready, RNG check");
    bool results[4]{};
    unsigned char entropy[64]{};
    results[0] = sceKernelGetRandomNumber(entropy, sizeof(entropy)) == 0;
    if (results[0]) { RAND_seed(entropy, sizeof(entropy)); results[0] = RAND_bytes(entropy, sizeof(entropy)) == 1; }
    step("thread init");
    td::init_openssl_threads();
    std::atomic<int> value{0};
    std::atomic<bool> release{false};
    td::set_thread_id(77);
    td::thread worker([&] {
        td::set_thread_id(88);
        value.store(td::get_thread_id());
        while (!release.load()) ::usleep(1000);
    });
    step("worker created");
    while (value.load() == 0) ::usleep(1000);
    bool isolated = td::get_thread_id() == 77;
    release.store(true);
    worker.join();
    step("worker joined");
    results[1] = value.load() == 88 && isolated;
    step("network init");
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    void* memory = std::malloc(4 * 1024 * 1024);
    SceNetInitParam param{}; param.memory = memory; param.size = 4 * 1024 * 1024;
    if (memory && sceNetInit(&param) == 0) {
        sceNetCtlInit();
        step("socket pair check");
        int fds[2];
        int pair_result = vita_socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
        bool sockets_ok = pair_result == 0;
        if (!sockets_ok) {
            char message[80]; std::snprintf(message, sizeof(message), "socket pair failed errno=%d", errno); step(message);
        } else {
            step("socket pair ready");
            td::NativeFd first(fds[0]), second(fds[1]);
            auto blocking = first.set_is_blocking_unsafe(false);
            sockets_ok = blocking.is_ok();
            if (!sockets_ok) step(blocking.message().str().c_str());
            if (sockets_ok) {
                auto socket = td::SocketFd::from_native_fd(std::move(first));
                sockets_ok = socket.is_ok();
                if (!sockets_ok) step(socket.error().message().str().c_str());
            }
        }
        if (sockets_ok) {
            step("event wakeup init");
            td::EventFd wake;
            wake.init();
            step("event wakeup release");
            wake.release();
            wake.wait(500);
            wake.acquire();
            wake.close();
            results[2] = true;
        }
    }
    sceIoMkdir("ux0:data/vita-tg", 0700);
    step("file and lock check");
    const td::string path = "ux0:data/vita-tg/port-test.tmp";
    step("opening session file");
    auto opened = td::FileFd::open(path, td::FileFd::Read | td::FileFd::Write | td::FileFd::Create | td::FileFd::Truncate);
    if (opened.is_ok()) {
        step("session file opened");
        auto file = opened.move_as_ok();
        step("locking session file");
        auto locked = file.lock(td::FileFd::LockFlags::Write, path, 1);
        auto other = td::FileFd::open(path, td::FileFd::Read | td::FileFd::Write);
        bool duplicate_rejected = false;
        if (other.is_ok()) {
            auto duplicate = other.move_as_ok();
            duplicate_rejected = duplicate.lock(td::FileFd::LockFlags::Write, path, 1).is_error();
        }
        step("writing session file");
        auto written = file.write(td::Slice("port-check"));
        results[3] = locked.is_ok() && duplicate_rejected && written.is_ok() && written.ok() == 10 && file.sync().is_ok();
        file.lock(td::FileFd::LockFlags::Unlock, path, 1).ignore();
        file.close();
        unlink(path.c_str());
    }
    step("checks complete");
    const char* names[] = {"System RNG and OpenSSL", "TDLib worker thread and TLS", "Loopback event wakeup", "Session file I/O and duplicate lock"};
    while (true) {
        vita2d_start_drawing(); vita2d_clear_screen();
        vita2d_pgf_draw_text(font, 30, 65, RGBA8(255,255,255,255), 1.2f, "Native Telegram platform checks");
        for (int i=0; i<4; ++i) vita2d_pgf_draw_textf(font, 30, 150+i*65, results[i] ? RGBA8(90,220,140,255) : RGBA8(255,90,90,255), 1.f, "%s: %s", names[i], results[i]?"PASS":"FAIL");
        vita2d_pgf_draw_text(font, 30, 485, RGBA8(255,255,255,255), 1.f, "Start: exit");
        vita2d_end_drawing(); vita2d_swap_buffers();
        sceKernelDelayThread(16000);
        SceCtrlData pad{}; sceCtrlPeekBufferPositive(0,&pad,1);
        if(pad.buttons & SCE_CTRL_START) break;
    }
    vita2d_wait_rendering_done(); vita2d_free_pgf(font); vita2d_fini();
    sceNetCtlTerm(); sceNetTerm(); std::free(memory);
    sceKernelExitProcess(0); return 0;
}
