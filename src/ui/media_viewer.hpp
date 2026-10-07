#pragma once
#include "telegram/auth.hpp"
#include <vita2d.h>
#include <thread>
#include <atomic>
#include <chrono>

namespace ui {
vita2d_texture* load_media_image(const std::string& path);
void fit_media(vita2d_texture* texture,float x,float y,float width,float height,float zoom = 1);
class MediaViewer {
public:
    ~MediaViewer() { close(); }
    void open(const telegram::Message& message,telegram::Auth& auth);
    void close();
    void update(telegram::Auth& auth);
    void controls(unsigned pressed);
    void draw(vita2d_pgf* font) const;
    bool active() const { return message_id_!=0; }
private:
    void stop_player();
    void start_video(const std::string& path);
    telegram::Message message_;
    telegram::Auth* owner_=nullptr;
    std::int64_t message_id_=0;
    // AVPlayer handles are opaque 32-bit values, including high-bit pointers.
    int player_=0;
    bool module_=false, attempted_=false, paused_=false, has_frame_=false;
    vita2d_texture* image_=nullptr;
    vita2d_texture frame_{};
    std::thread audio_;
    std::atomic<bool> stop_audio_{false};
    std::string status_;
    float zoom_=1;
    std::chrono::steady_clock::time_point started_;
};
}
