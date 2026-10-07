#pragma once
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <thread>
using SceBool=int;
constexpr int SCE_TRUE=1;
struct SceGxmTexture { void* data=nullptr; unsigned width=0,height=0; };
struct vita2d_texture { SceGxmTexture gxm_tex; };
struct vita2d_pgf {};
constexpr int SCE_GXM_TEXTURE_FILTER_LINEAR=1, SCE_GXM_MEMORY_ATTRIB_READ=1, SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC1=1;
constexpr int SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW=1, SCE_SYSMODULE_AVPLAYER=0x4c;
constexpr unsigned SCE_CTRL_CIRCLE=1, SCE_CTRL_CROSS=2, SCE_CTRL_RTRIGGER=4, SCE_CTRL_LTRIGGER=8;
constexpr int SCE_AUDIO_OUT_PORT_TYPE_BGM=1, SCE_AUDIO_OUT_MODE_MONO=0, SCE_AUDIO_OUT_MODE_STEREO=1;
using SceAvPlayerHandle=int;
using Alloc=void*(*)(void*,uint32_t,uint32_t);
using Free=void(*)(void*,void*);
struct SceAvPlayerMemReplacement { void* objectPointer; Alloc allocate; Free deallocate; Alloc allocateTexture; Free deallocateTexture; };
struct SceAvPlayerInitData { SceAvPlayerMemReplacement memoryReplacement{}; unsigned basePriority=0; int numOutputVideoFrameBuffers=0; SceBool autoStart=0; };
struct AudioInfo { unsigned channelCount=0,sampleRate=0,size=0; };
struct VideoInfo { unsigned width=0,height=0; float aspectRatio=0; };
struct SceAvPlayerFrameInfo { uint8_t* pData=nullptr; struct { AudioInfo audio; VideoInfo video; } details; };
int sceSysmoduleLoadModule(int);
int sceSysmoduleUnloadModule(int);
int sceAvPlayerInit(SceAvPlayerInitData*);
int sceAvPlayerAddSource(int,const char*);
int sceAvPlayerClose(int);
int sceAvPlayerStop(int);
int sceAvPlayerPause(int);
int sceAvPlayerResume(int);
int sceAvPlayerSetLooping(int,int);
SceBool sceAvPlayerGetAudioData(int,SceAvPlayerFrameInfo*);
SceBool sceAvPlayerGetVideoData(int,SceAvPlayerFrameInfo*);
SceBool sceAvPlayerIsActive(int);
int sceAudioOutOpenPort(int,unsigned,unsigned,int);
int sceAudioOutReleasePort(int);
int sceAudioOutOutput(int,const void*);
int sceKernelAllocMemBlock(const char*,int,unsigned,void*);
int sceKernelFreeMemBlock(int);
int sceKernelGetMemBlockBase(int,void**);
int sceKernelFindMemBlockByAddr(void*,unsigned);
int sceGxmMapMemory(void*,unsigned,int);
int sceGxmUnmapMemory(void*);
int sceGxmTextureInitLinear(SceGxmTexture*,void*,int,unsigned,unsigned,unsigned);
void vita2d_wait_rendering_done();
void vita2d_free_texture(vita2d_texture*);
void vita2d_texture_set_filters(vita2d_texture*,int,int);
unsigned vita2d_texture_get_width(vita2d_texture*);
unsigned vita2d_texture_get_height(vita2d_texture*);
void vita2d_draw_texture_scale(vita2d_texture*,float,float,float,float);
void vita2d_draw_rectangle(float,float,float,float,unsigned);
void vita2d_pgf_draw_text(vita2d_pgf*,int,int,unsigned,float,const char*);
inline void sceKernelDelayThread(unsigned us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }
inline void* memalign(unsigned alignment,unsigned size) { void* p=nullptr; return posix_memalign(&p,alignment,size)==0 ? p : nullptr; }
