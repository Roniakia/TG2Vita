#pragma once
#include <cstdint>
namespace telegram::config {
constexpr unsigned initial_history_messages = 10;
constexpr int history_request_limit = 50;
constexpr unsigned media_file_limit = 128;
constexpr std::int64_t media_preview_bytes = 2*1024*1024, media_photo_bytes = 8*1024*1024, media_video_bytes = 64*1024*1024;
constexpr unsigned avatar_file_limit = 2000;
constexpr int chat_list_batch = 100, chat_list_snapshot_limit = 2000;
}
