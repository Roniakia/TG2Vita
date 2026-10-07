#pragma once
#include <array>
#include <cstdint>

// Edit this file to tune the UI, then rebuild. Dimensions are Vita screen pixels.
namespace ui::theme {
constexpr std::uint32_t rgba(unsigned r, unsigned g, unsigned b, unsigned a = 255) {
    return r | (g << 8) | (b << 16) | (a << 24);
}
constexpr auto background = rgba(16,23,31);
constexpr auto panel = rgba(24,34,45);
constexpr auto accent = rgba(62,166,221);
constexpr auto white = rgba(235,241,247);
constexpr auto muted = rgba(154,172,189);
constexpr auto success = rgba(72,211,161);
constexpr auto error_color = rgba(255,143,143);
constexpr auto focus = rgba(37,62,80);
constexpr auto avatar_color = rgba(44,87,112);
constexpr auto outgoing = rgba(35,88,118);
constexpr int screen_width = 960, screen_height = 544;
constexpr int header_height = 72, sidebar_width = 300, footer_top = 488;
constexpr int content_x = 330, content_width = 590;
constexpr int sidebar_top = 133, sidebar_step = 70, sidebar_row_height = 58;
constexpr int list_top = 151, list_step = 57, list_row_height = 51, list_page_size = 5;
constexpr int conversation_top = 140, conversation_bottom = 453;
constexpr int bubble_width = 405, bubble_padding = 15, bubble_gap = 11;
constexpr int bubble_text_baseline = 27, bubble_line_height = 24, bubble_metadata_height = 41;
constexpr int sender_line_height = 22, message_avatar_size = 40, message_avatar_gap = 10;
constexpr int media_preview_height = 184, media_label_height = 28;
constexpr unsigned media_texture_limit = 4;
constexpr unsigned avatar_texture_limit = 16;
constexpr int emoji_size = 22, emoji_advance = 24, emoji_baseline_offset = 19;
constexpr int bubble_focus_border = 3, corner_radius = 12;
constexpr float message_scale = 0.9f, timestamp_scale = 0.65f;
constexpr int history_prefetch_distance = 2*(conversation_bottom-conversation_top); // Two screens ahead.
constexpr int scroll_step = 48, touch_slop = 10, touch_drag_step = 2, touch_horizontal_slop = 24;
constexpr int right_stick_deadzone = 24;
constexpr float right_stick_scroll_speed = 420.0f; // Pixels per second at full tilt.
constexpr int list_swipe_step = 36, avatar_radius = 20;
constexpr const char* timestamp_format = "%d %b  %H:%M";
constexpr std::array<const char*,4> items = {"Chats","Contacts","Saved Messages","Settings"};
}
