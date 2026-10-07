#include "ui/media_viewer.hpp"
#include <vita_platform.hpp>
#include <cassert>
#include <iostream>
#include <mutex>
#include <vector>
#include <cstring>
namespace {
constexpr int hardware_handle=static_cast<int>(0x84312c90u);
int returned_handle=hardware_handle, source_result=0, module_result=0;
bool owned=false, loaded=false, produced=false, playing=true, invalid_frame=false;
unsigned pause_count=0,resume_count=0,video_reads=0;
std::atomic<unsigned> audio_reads{0};
std::vector<std::string> calls;
}
namespace telegram {
Auth::~Auth() {}
void Auth::cancel_media(int) {}
void Auth::request_media(const Message&,bool,bool) {}
MediaFile Auth::media_file(int) const { MediaFile f; f.path="/mock/video.mp4"; return f; }
struct AuthDataTestAccess {
    static void ready(Auth& auth) { auth.stage_=AuthStage::Ready; }
    static void set(Auth& auth,const Message& message) { auth.messages_={message}; }
    static void logout(Auth& auth) { auth.stage_=AuthStage::Closing; }
    static void remove(Auth& auth) { auth.messages_.clear(); }
};
}
int sceSysmoduleLoadModule(int) { calls.push_back("load"); loaded=module_result>=0; return module_result; }
int sceSysmoduleUnloadModule(int) { assert(!owned); calls.push_back("unload"); loaded=false; return 0; }
int sceAvPlayerInit(SceAvPlayerInitData* init) {
    assert(loaded && init->memoryReplacement.allocate && init->memoryReplacement.deallocate && init->autoStart);
    calls.push_back("init");
    owned=returned_handle!=0 && returned_handle!=static_cast<int>(0x806a0003u) && returned_handle!=-1;
    return returned_handle;
}
int sceAvPlayerAddSource(int h,const char*) { assert(h==hardware_handle && owned && loaded); calls.push_back("source"); return source_result; }
int sceAvPlayerClose(int h) { assert(h==hardware_handle && loaded && owned); calls.push_back("close"); owned=false; return 0; }
int sceAvPlayerStop(int h) { assert(h==hardware_handle && owned && loaded); calls.push_back("stop"); return 0; }
int sceAvPlayerPause(int h) { assert(h==hardware_handle && owned); ++pause_count; return 0; }
int sceAvPlayerResume(int h) { assert(h==hardware_handle && owned); ++resume_count; return 0; }
int sceAvPlayerSetLooping(int h,int) { assert(h==hardware_handle && owned); return 0; }
SceBool sceAvPlayerGetAudioData(int h,SceAvPlayerFrameInfo*) { assert(h==hardware_handle && owned && loaded); ++audio_reads; return 0; }
SceBool sceAvPlayerGetVideoData(int h,SceAvPlayerFrameInfo* frame) {
    assert(h==hardware_handle && owned && loaded); ++video_reads;
    static uint8_t pixels[16]{};
    if (produced) return 0;
    produced=true; frame->pData=pixels; frame->details.video.width=invalid_frame ? 1921 : 2; frame->details.video.height=2; return 1;
}
SceBool sceAvPlayerIsActive(int h) { assert(h==hardware_handle && owned); return playing; }
int sceAudioOutOpenPort(int,unsigned,unsigned,int) { return 1; }
int sceAudioOutReleasePort(int) { return 0; }
int sceAudioOutOutput(int,const void*) { return 0; }
int sceKernelAllocMemBlock(const char*,int,unsigned,void*) { return -1; }
int sceKernelFreeMemBlock(int) { return 0; }
int sceKernelGetMemBlockBase(int,void**) { return -1; }
int sceKernelFindMemBlockByAddr(void*,unsigned) { return -1; }
int sceGxmMapMemory(void*,unsigned,int) { return 0; }
int sceGxmUnmapMemory(void*) { return 0; }
int sceGxmTextureInitLinear(SceGxmTexture* t,void* ptr,int,unsigned w,unsigned h,unsigned) { t->data=ptr; t->width=w; t->height=h; return 0; }
void vita2d_wait_rendering_done() {}
void vita2d_free_texture(vita2d_texture* t) { delete t; }
void vita2d_texture_set_filters(vita2d_texture*,int,int) {}
unsigned vita2d_texture_get_width(vita2d_texture* t) { return t->gxm_tex.width; }
unsigned vita2d_texture_get_height(vita2d_texture* t) { return t->gxm_tex.height; }
void vita2d_draw_texture_scale(vita2d_texture*,float,float,float,float) {}
void vita2d_draw_rectangle(float,float,float,float,unsigned) {}
void vita2d_pgf_draw_text(vita2d_pgf*,int,int,unsigned,float,const char*) {}
namespace ui { vita2d_texture* load_media_image(const std::string&) { return nullptr; } }
int main() {
    using Access=telegram::AuthDataTestAccess;
    telegram::Auth auth; Access::ready(auth);
    telegram::Message message; message.id=500; message.media.kind=telegram::MediaKind::Video; message.media.file_id=10;
    Access::set(auth,message);
    ui::MediaViewer viewer;
    assert(hardware_handle<0); // Actual pointer-shaped handle seen in the hardware dump.
    viewer.open(message,auth); viewer.update(auth);
    assert(owned && loaded && video_reads==1);
    viewer.controls(SCE_CTRL_CROSS); assert(pause_count==1);
    viewer.update(auth); assert(video_reads==1);
    viewer.controls(SCE_CTRL_CROSS); assert(resume_count==1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(audio_reads>0); // Verify a running audio reader is joined before Close.
    viewer.controls(SCE_CTRL_CIRCLE);
    assert(!viewer.active() && !owned && !loaded);
    assert((calls==std::vector<std::string>{"load","init","source","stop","close","unload"}));
    calls.clear(); source_result=-1;
    viewer.open(message,auth); viewer.update(auth);
    assert(!owned && !loaded && (calls==std::vector<std::string>{"load","init","source","stop","close","unload"}));
    viewer.close(); source_result=0;
    for (int failure : {0,static_cast<int>(0x806a0003u),-1}) {
        calls.clear(); returned_handle=failure;
        viewer.open(message,auth); viewer.update(auth); viewer.close();
        assert(!owned && !loaded && (calls==std::vector<std::string>{"load","init","unload"}));
    }
    returned_handle=hardware_handle; calls.clear(); produced=false;
    viewer.open(message,auth); viewer.update(auth); Access::logout(auth); viewer.update(auth);
    assert(!viewer.active() && !owned && !loaded && calls.back()=="unload");
    Access::ready(auth); calls.clear(); produced=false;
    viewer.open(message,auth); viewer.update(auth); Access::remove(auth); viewer.update(auth);
    assert(!viewer.active() && !owned && !loaded);
    Access::set(auth,message); calls.clear(); produced=false; playing=false;
    viewer.open(message,auth); viewer.update(auth);
    assert(!owned && !loaded && calls.back()=="unload"); viewer.close();
    playing=true; invalid_frame=true; produced=false; calls.clear();
    viewer.open(message,auth); viewer.update(auth); viewer.close();
    assert(!owned && !loaded && calls.back()=="unload"); invalid_frame=false;
    calls.clear(); produced=false;
    {
        ui::MediaViewer active_at_shutdown;
        active_at_shutdown.open(message,auth); active_at_shutdown.update(auth);
        assert(owned && loaded);
    }
    assert(!owned && !loaded && calls.back()=="unload");
    module_result=-1; calls.clear();
    viewer.open(message,auth); viewer.update(auth); viewer.close();
    assert((calls==std::vector<std::string>{"load"}));
    std::cout<<"Native player opaque handle, playback, pause/resume, failures and teardown regression tests passed\n";
}
