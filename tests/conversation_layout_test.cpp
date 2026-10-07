#include "ui/conversation_layout.hpp"
#include <cassert>
#include <iostream>
int main() {
    assert(ui::stick_scroll(128,1)==0);
    assert(ui::stick_scroll(128-ui::theme::right_stick_deadzone,1)==0);
    assert(ui::stick_scroll(0,1)>0 && ui::stick_scroll(255,1)<0);
    assert(ui::stick_scroll(0,0.5f)*2==ui::stick_scroll(0,1));
    auto measure=[](const std::string& s) { return s.size()*10.0f; };
    const std::string long_text(5000,'x');
    auto lines=ui::wrap_message(long_text,measure);
    std::string restored;
    for (const auto& line:lines) { restored+=line; assert(measure(line)<=ui::theme::bubble_width-ui::theme::bubble_padding*2); }
    assert(restored==long_text && lines.size()>2);
    auto paragraphs=ui::wrap_message("First\n\nLast",measure);
    assert(paragraphs.size()==3 && paragraphs[1].empty() && paragraphs[2]=="Last");
    const std::string unicode="Привет 🌍";
    auto utf8=ui::wrap_message(unicode,measure);
    assert(utf8.size()==1 && utf8[0]==unicode);
    for (const std::string sequence : {"😀", "👍🏽", "🇸🇪", "👨‍👩‍👧‍👦", "❤️", "1️⃣", "🏳️‍🌈"}) {
        const auto token=ui::emoji::next(sequence,0);
        assert(token.tile>=0 && token.bytes==sequence.size());
        assert(ui::emoji::width(sequence,24,measure)==24);
        auto emoji_measure=[&](const std::string& text) { return ui::emoji::width(text,24,measure); };
        std::string repeated;
        for (int i=0;i<40;++i) repeated+=sequence;
        std::string joined;
        for (const auto& line : ui::wrap_message(repeated,emoji_measure)) {
            assert(emoji_measure(line)<=ui::theme::bubble_width-ui::theme::bubble_padding*2);
            for (std::size_t i=0;i<line.size();) { auto t=ui::emoji::next(line,i); assert(t.bytes==sequence.size()); i+=t.bytes; }
            joined+=line;
        }
        assert(joined==repeated);
    }
    assert(ui::emoji::next("❤︎",0).tile<0);
    assert(ui::emoji::next("Hello",0).tile<0);
    assert(ui::emoji::width("A😀B",24,measure)==44);
    std::vector<ui::BubbleLayout> bubbles(2);
    bubbles[0].lines=lines; bubbles[1].lines=ui::wrap_message("Older",measure);
    bubbles[0].title_lines={"Ada Lovelace"}; bubbles[0].forward_lines={"Forwarded from Alice"};
    const auto height=ui::position_bubbles(bubbles);
    const auto maximum=ui::maximum_scroll(height);
    assert(maximum>0 && bubbles[0].height>ui::theme::conversation_bottom-ui::theme::conversation_top);
    assert(ui::bubble_top(bubbles[0],0)+bubbles[0].height==ui::theme::conversation_bottom);
    assert(ui::bubble_top(bubbles.back(),maximum)==ui::theme::conversation_top);
    assert(ui::hit_bubble(bubbles,0,ui::theme::conversation_bottom-1)==0);
    assert(ui::hit_bubble(bubbles,maximum,ui::theme::conversation_top+1)==1);
    assert(ui::maximum_scroll(10)==0);
    assert(ui::near_history_edge(100,0));
    assert(!ui::near_history_edge(height,0));
    assert(ui::near_history_edge(height,maximum-ui::theme::history_prefetch_distance));
    assert(!ui::near_history_edge(height,maximum-ui::theme::history_prefetch_distance-1));
    const auto text_height=bubbles[0].height;
    bubbles[0].media_height=ui::theme::media_preview_height+ui::theme::media_label_height;
    const auto media_height=ui::position_bubbles(bubbles);
    assert(bubbles[0].height==text_height+bubbles[0].media_height);
    assert(ui::maximum_scroll(media_height)>maximum);
    assert(ui::hit_bubble(bubbles,0,ui::theme::conversation_bottom-1)==0);
    std::string caption;
    for (const auto& line : bubbles[0].lines) caption+=line;
    assert(caption==long_text);
    std::cout<<"Full-text wrapping and variable-height scrolling tests passed\n";
}
