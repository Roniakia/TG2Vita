#include "media_viewer.hpp"
#include "theme.hpp"
#include <psp2/avplayer.h>
#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <cstdio>
#include <cstdlib>
#include <malloc.h>
#include <cstring>
#include <algorithm>

namespace ui {
namespace {
void* allocate(void*,uint32_t alignment,uint32_t size) { return memalign(std::max(8u,alignment),size); }
void deallocate(void*,void* ptr) { free(ptr); }
void* allocate_frame(void*,uint32_t,uint32_t size) {
    // AVPlayer writes NV12 frames; the GPU must be able to address them.
    const auto bytes=(size+0x3ffffu)&~0x3ffffu;
    if (!bytes || bytes>16*1024*1024) return nullptr;
    const auto uid=sceKernelAllocMemBlock("tg-video",SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,bytes,nullptr);
    if (uid<0) return nullptr;
    void* ptr=nullptr;
    if (sceKernelGetMemBlockBase(uid,&ptr)<0 || sceGxmMapMemory(ptr,bytes,SCE_GXM_MEMORY_ATTRIB_READ)<0) { sceKernelFreeMemBlock(uid); return nullptr; }
    return ptr;
}
void free_frame(void*,void* ptr) {
    if (!ptr) return;
    const auto uid=sceKernelFindMemBlockByAddr(ptr,0);
    sceGxmUnmapMemory(ptr); if (uid>=0) sceKernelFreeMemBlock(uid);
}
}
void fit_media(vita2d_texture* texture,float x,float y,float width,float height,float zoom) {
    if (!texture) return;
    const float w=vita2d_texture_get_width(texture),h=vita2d_texture_get_height(texture);
    if (w<=0 || h<=0) return;
    const auto scale=std::min(width/w,height/h)*zoom;
    vita2d_draw_texture_scale(texture,x+(width-w*scale)/2,y+(height-h*scale)/2,scale,scale);
}
void MediaViewer::open(const telegram::Message& message,telegram::Auth& auth) {
    if (message.media.kind==telegram::MediaKind::None) return;
    close(); owner_=&auth; message_=message; message_id_=message.id; status_="Downloading media...";
    if (message.media.secret) { status_="Self-destructing media is not supported."; attempted_=true; return; }
    if (message.media.kind==telegram::MediaKind::Unsupported || !message.media.file_id) { status_="This attachment format is not supported."; attempted_=true; return; }
    auth.request_media(message,true,true);
}
void MediaViewer::stop_player() {
    stop_audio_=true;
    if (audio_.joinable()) audio_.join();
    vita2d_wait_rendering_done();
    if (player_!=0) { sceAvPlayerStop(player_); sceAvPlayerClose(player_); player_=0; }
    if (module_) { sceSysmoduleUnloadModule(SCE_SYSMODULE_AVPLAYER); module_=false; }
    has_frame_=false; frame_={};
}
void MediaViewer::close() {
    if (owner_) owner_->cancel_media(message_.media.file_id);
    owner_=nullptr;
    stop_player();
    if (image_) vita2d_free_texture(image_);
    image_=nullptr; message_id_=0; attempted_=paused_=false; zoom_=1; status_.clear();
}
void MediaViewer::start_video(const std::string& path) {
    if (sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER)<0) { status_="Native video player unavailable."; return; }
    module_=true;
    SceAvPlayerInitData init{};
    init.memoryReplacement={nullptr,allocate,deallocate,allocate_frame,free_frame};
    init.basePriority=0xa0; init.numOutputVideoFrameBuffers=3; init.autoStart=SCE_TRUE;
    const int handle=sceAvPlayerInit(&init);
    // Real Vita returns a heap pointer with its high bit set. The SDK's signed
    // typedef must not turn a live player into an error or bypass its Close.
    // Null and AVPlayer's error-code namespace are not owned player instances.
    const auto bits=static_cast<std::uint32_t>(handle);
    if (!bits || bits==0xffffffffu || (bits&0xffff0000u)==0x806a0000u) {
        stop_player(); status_="Native video player initialization failed."; return;
    }
    player_=handle;
    if (sceAvPlayerAddSource(player_,path.c_str())<0) { stop_player(); status_="Vita cannot play this video format."; return; }
    if (message_.media.kind==telegram::MediaKind::Animation) sceAvPlayerSetLooping(player_,SCE_TRUE);
    started_=std::chrono::steady_clock::now(); status_="Opening video...";
    stop_audio_=false;
    audio_=std::thread([this] {
        int port=-1; unsigned rate=0,channels=0,samples=0;
        while (!stop_audio_) {
            SceAvPlayerFrameInfo audio{};
            if (!sceAvPlayerGetAudioData(player_,&audio)) { sceKernelDelayThread(5000); continue; }
            const auto& info=audio.details.audio;
            if (!audio.pData || !info.size || (info.channelCount!=1 && info.channelCount!=2)) continue;
            const auto length=info.size/(2*info.channelCount);
            if (!length || length%64 || length>65472) continue;
            if (port<0 || rate!=info.sampleRate || channels!=info.channelCount || samples!=length) {
                if (port>=0) sceAudioOutReleasePort(port);
                rate=info.sampleRate; channels=info.channelCount; samples=length;
                port=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM,length,rate,channels==1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
            }
            if (port>=0) sceAudioOutOutput(port,audio.pData);
        }
        if (port>=0) { sceAudioOutOutput(port,nullptr); sceAudioOutReleasePort(port); }
    });
}
void MediaViewer::update(telegram::Auth& auth) {
    if (!active()) return;
    const auto found=std::find_if(auth.messages().begin(),auth.messages().end(),[this](const telegram::Message& m) { return m.id==message_id_; });
    if (auth.stage()!=telegram::AuthStage::Ready || found==auth.messages().end()) { close(); return; }
    if (found->media.file_id!=message_.media.file_id || found->media.kind!=message_.media.kind || found->media.secret!=message_.media.secret) { close(); return; }
    if (!attempted_) {
        const auto file=auth.media_file(message_.media.file_id);
        if (file.failed) status_="Download failed or media exceeds Vita limits. Circle: back; Cross: retry.";
        else if (!file.path.empty()) {
            attempted_=true;
            if (message_.media.kind==telegram::MediaKind::Photo) { image_=load_media_image(file.path); status_=image_ ? "" : "Image format or dimensions are not supported."; }
            else start_video(file.path);
        } else if (file.size>0) status_="Downloading: "+std::to_string(std::min<std::int64_t>(100,file.downloaded*100/file.size))+"%";
        else status_="Waiting for media download...";
    }
    if (player_!=0 && !paused_) {
        // Finish the previous draw before AVPlayer can reuse its frame buffers.
        vita2d_wait_rendering_done();
        SceAvPlayerFrameInfo video{};
        if (sceAvPlayerGetVideoData(player_,&video)) {
            const auto& info=video.details.video;
            if (!video.pData || !info.width || !info.height || info.width>1920 || info.height>1088 ||
                sceGxmTextureInitLinear(&frame_.gxm_tex,video.pData,SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC1,info.width,info.height,0)<0) {
                stop_player(); status_="Unsupported native video frame."; return;
            }
            vita2d_texture_set_filters(&frame_,SCE_GXM_TEXTURE_FILTER_LINEAR,SCE_GXM_TEXTURE_FILTER_LINEAR);
            has_frame_=true; status_.clear();
        }
        if (!sceAvPlayerIsActive(player_) && has_frame_) { stop_player(); status_="Playback finished. Circle: back."; }
        else if (!has_frame_ && std::chrono::steady_clock::now()-started_>std::chrono::seconds(10)) { stop_player(); status_="Vita cannot decode this video. Circle: back."; }
    }
}
void MediaViewer::controls(unsigned pressed) {
    if (pressed&SCE_CTRL_CIRCLE) { close(); return; }
    if (!attempted_ && owner_ && (pressed&SCE_CTRL_CROSS) && owner_->media_file(message_.media.file_id).failed)
        owner_->request_media(message_,true,true);
    if (image_) { if (pressed&SCE_CTRL_RTRIGGER) zoom_=std::min(3.0f,zoom_+0.25f); if (pressed&SCE_CTRL_LTRIGGER) zoom_=std::max(1.0f,zoom_-0.25f); }
    if (player_!=0 && (pressed&SCE_CTRL_CROSS)) {
        vita2d_wait_rendering_done();
        const int result=paused_ ? sceAvPlayerResume(player_) : sceAvPlayerPause(player_);
        if (result>=0) paused_=!paused_;
    }
}
void MediaViewer::draw(vita2d_pgf* font) const {
    vita2d_draw_rectangle(0,0,960,544,theme::rgba(0,0,0));
    fit_media(image_ ? image_ : has_frame_ ? const_cast<vita2d_texture*>(&frame_) : nullptr,0,0,960,496,zoom_);
    if (!status_.empty()) vita2d_pgf_draw_text(font,30,255,theme::white,0.85f,status_.c_str());
    vita2d_draw_rectangle(0,496,960,48,theme::panel);
    vita2d_pgf_draw_text(font,24,526,theme::muted,0.85f,image_ ? "Circle: back   L / R: zoom" : "Circle: back   Cross: pause / resume");
}
}
