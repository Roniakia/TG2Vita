#pragma once
#include "theme.hpp"
#include "emoji.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace ui {
// The same geometry drives painting, touch hit testing and scrolling.
struct BubbleLayout {
    std::vector<std::string> lines;
    float height = 0, bottom_offset = 0;
    float media_height = 0;
    std::vector<std::string> title_lines, forward_lines;
};
template<class Measure>
std::vector<std::string> wrap_message(const std::string& value, Measure measure) {
    std::vector<std::string> result;
    std::string line;
    const float width = theme::bubble_width-theme::bubble_padding*2;
    for (std::size_t i=0;i<value.size();) {
        auto end=i+emoji::next(value,i).bytes;
        const auto character=value.substr(i,end-i); i=end;
        if (character=="\r") continue;
        if (character=="\n") { result.push_back(line); line.clear(); continue; }
        if (!line.empty() && measure(line+character)>width) {
            const auto space=line.find_last_of(' ');
            if (space!=std::string::npos && space>0) {
                result.push_back(line.substr(0,space)); line=line.substr(space+1);
            } else { result.push_back(line); line.clear(); }
            if (!line.empty() && measure(line+character)>width) { result.push_back(line); line.clear(); }
        }
        line+=character;
    }
    result.push_back(line);
    return result;
}
inline float position_bubbles(std::vector<BubbleLayout>& bubbles) {
    float offset=0;
    for (auto& bubble : bubbles) {
        bubble.height=bubble.media_height+bubble.lines.size()*theme::bubble_line_height+theme::bubble_metadata_height+
            (bubble.title_lines.size()+bubble.forward_lines.size())*theme::sender_line_height;
        bubble.bottom_offset=offset;
        offset+=bubble.height+theme::bubble_gap;
    }
    return bubbles.empty() ? 0 : offset-theme::bubble_gap;
}
inline float stick_scroll(int axis,float seconds) {
    const int deflection=axis-128;
    const int distance=std::abs(deflection);
    if (distance<=theme::right_stick_deadzone) return 0;
    const float magnitude=std::min(1.0f,static_cast<float>(distance-theme::right_stick_deadzone)/(127-theme::right_stick_deadzone));
    // Up reveals older messages above the current viewport.
    return (deflection<0 ? 1 : -1)*magnitude*theme::right_stick_scroll_speed*seconds;
}
inline float maximum_scroll(float height) {
    return std::max(0.0f,height-(theme::conversation_bottom-theme::conversation_top));
}
inline bool near_history_edge(float height,float scroll) {
    return maximum_scroll(height)-scroll<=theme::history_prefetch_distance;
}
inline float bubble_top(const BubbleLayout& bubble,float scroll) {
    return theme::conversation_bottom+scroll-bubble.bottom_offset-bubble.height;
}
inline std::size_t hit_bubble(const std::vector<BubbleLayout>& bubbles,float scroll,float y) {
    for (std::size_t i=0;i<bubbles.size();++i) {
        const auto top=bubble_top(bubbles[i],scroll);
        if (y>=top && y<top+bubbles[i].height) return i;
    }
    return bubbles.size();
}
}
