#include "version.hpp"
#include "app_view.hpp"
#include <algorithm>
#include <ctime>

namespace ui {
using namespace ui::theme;

namespace { vita2d_texture* emoji_atlas=nullptr; }
void init_emoji() {
    emoji_atlas=vita2d_load_PNG_file("app0:assets/emoji/atlas.png");
    if (emoji_atlas) vita2d_texture_set_filters(emoji_atlas,SCE_GXM_TEXTURE_FILTER_LINEAR,SCE_GXM_TEXTURE_FILTER_LINEAR);
}
void clear_emoji() {
    vita2d_wait_rendering_done();
    if (emoji_atlas) vita2d_free_texture(emoji_atlas);
    emoji_atlas=nullptr;
}
float text_width(vita2d_pgf* font,float scale,const std::string& value) {
    return emoji::width(value,emoji_advance*scale/message_scale,[&](const std::string& run) {
        return vita2d_pgf_text_width(font,scale,run.c_str());
    });
}
void text(vita2d_pgf* font, int x, int y, const char* value, unsigned int color = white, float scale = 1.0f) {
    const std::string input=value;
    float cursor=x;
    std::string run;
    auto flush=[&]() {
        vita2d_pgf_draw_text(font,static_cast<int>(cursor),y,color,scale,run.c_str());
        cursor+=vita2d_pgf_text_width(font,scale,run.c_str()); run.clear();
    };
    for (std::size_t i=0;i<input.size();) {
        const auto token=emoji::next(input,i);
        if (token.tile>=0) {
            flush();
            const float ratio=scale/message_scale;
            if (emoji_atlas) vita2d_draw_texture_part_scale(emoji_atlas,cursor,y-emoji_baseline_offset*ratio,
                (token.tile%64)*24+1,(token.tile/64)*24+1,22,22,emoji_size*ratio/22,emoji_size*ratio/22);
            else vita2d_draw_rectangle(cursor,y-emoji_baseline_offset*ratio,emoji_size*ratio,emoji_size*ratio,muted);
            cursor+=emoji_advance*ratio;
        } else run+=input.substr(i,token.bytes);
        i+=token.bytes;
    }
    flush();
}
void paragraph(vita2d_pgf* font, int y, const std::string& value, unsigned int color, int max_lines = 3) {
    std::size_t start = 0;
    for (int line = 0; start < value.size() && line < max_lines; ++line) {
        std::size_t end = std::min(start + 48, value.size());
        if (end < value.size()) {
            const auto space = value.rfind(' ', end);
            if (space != std::string::npos && space > start) end = space;
        }
        text(font, content_x, y + line * 31, value.substr(start, end - start).c_str(), color, 0.9f);
        start = end;
        while (start < value.size() && value[start] == ' ') ++start;
    }
}
void clipped(vita2d_pgf* font, int x, int y, std::string value, unsigned int color, float scale, float width) {
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    if (text_width(font, scale, value) > width) {
        while (!value.empty() && text_width(font, scale, value + "...") > width) {
            std::size_t last=0;
            for (std::size_t i=0;i<value.size();) { last=i; i+=emoji::next(value,i).bytes; }
            value.resize(last);
        }
        value += "...";
    }
    text(font,x,y,value.c_str(),color,scale);
}
void Conversation::sync(vita2d_pgf* font,const telegram::Auth& auth) {
    const auto& messages=auth.messages();
    std::vector<std::string> next_titles,next_forwards;
    for (const auto& message : messages) { next_titles.push_back(auth.sender_name(message)); next_forwards.push_back(auth.forwarding_title(message)); }
    if (titles==next_titles && forwards==next_forwards && source.size()==messages.size() && std::equal(source.begin(),source.end(),messages.begin(),
        [](const telegram::Message& a,const telegram::Message& b) { return a.id==b.id && a.text==b.text && a.date==b.date && a.outgoing==b.outgoing && a.media.kind==b.media.kind && a.media.file_id==b.media.file_id && a.media.preview_id==b.media.preview_id && a.media.spoiler==b.media.spoiler && a.media.secret==b.media.secret; })) return;
    std::map<std::int64_t,std::size_t> previous;
    for (std::size_t i=0;i<source.size();++i) previous[source[i].id]=i;
    std::vector<BubbleLayout> next;
    auto measure=[font](const std::string& value) { return text_width(font,message_scale,value); };
    for (std::size_t i=0;i<messages.size();++i) {
        BubbleLayout bubble;
        const auto old=previous.find(messages[i].id);
        if (old!=previous.end() && source[old->second].text==messages[i].text)
            bubble.lines=std::move(bubbles[old->second].lines);
        else bubble.lines=wrap_message(messages[i].text,measure);
        if (messages[i].media.kind!=telegram::MediaKind::None) bubble.media_height=media_preview_height+media_label_height;
        bubble.title_lines=wrap_message(next_titles[i],measure);
        if (!next_forwards[i].empty()) bubble.forward_lines=wrap_message(next_forwards[i],measure);
        next.push_back(std::move(bubble));
    }
    titles=std::move(next_titles); forwards=std::move(next_forwards);
    source=messages; bubbles=std::move(next);
    height=position_bubbles(bubbles);
}
struct AvatarTexture { std::string path; vita2d_texture* texture=nullptr; };
std::vector<AvatarTexture> avatar_textures;
void clear_avatars() {
    vita2d_wait_rendering_done();
    for (const auto& avatar : avatar_textures) if (avatar.texture) vita2d_free_texture(avatar.texture);
    avatar_textures.clear();
}
void prepare_avatars(const Navigation& nav,const telegram::Auth& auth,const Conversation& conversation) {
    if (auth.stage()!=telegram::AuthStage::Ready) { if (!avatar_textures.empty()) clear_avatars(); return; }
    if (!nav.history) return;
    std::vector<std::string> visible;
    for (std::size_t i=0;i<conversation.bubbles.size();++i) {
        const auto top=bubble_top(conversation.bubbles[i],nav.scroll);
        if (top<theme::conversation_bottom && top+conversation.bubbles[i].height>theme::conversation_top) {
            const auto path=auth.avatar_path(auth.messages()[i]);
            if (!path.empty()) visible.push_back(path);
        }
    }
    // Evict before issuing this frame's draw commands, after prior GPU work.
    vita2d_wait_rendering_done();
    for (const auto& path : visible) {
        if (std::any_of(avatar_textures.begin(),avatar_textures.end(),[&](const AvatarTexture& avatar) { return avatar.path==path; })) continue;
        if (avatar_textures.size()>=avatar_texture_limit) {
            const auto victim=std::find_if(avatar_textures.begin(),avatar_textures.end(),[&](const AvatarTexture& avatar) { return std::find(visible.begin(),visible.end(),avatar.path)==visible.end(); });
            if (victim==avatar_textures.end()) break;
            if (victim->texture) vita2d_free_texture(victim->texture);
            avatar_textures.erase(victim);
        }
        auto texture=vita2d_load_JPEG_file(path.c_str());
        if (!texture) texture=vita2d_load_PNG_file(path.c_str());
        avatar_textures.push_back({path,texture});
    }
}
std::vector<AvatarTexture> media_textures;
void clear_media() {
    vita2d_wait_rendering_done();
    for (auto& item : media_textures) if (item.texture) vita2d_free_texture(item.texture);
    media_textures.clear();
}
void prepare_media(const Navigation& nav,const telegram::Auth& auth,const Conversation& conversation) {
    if (auth.stage()!=telegram::AuthStage::Ready || !nav.history) { if (!media_textures.empty()) clear_media(); return; }
    std::vector<std::string> visible;
    for (std::size_t i=0;i<conversation.bubbles.size();++i) {
        const auto top=bubble_top(conversation.bubbles[i],nav.scroll);
        const auto& media=auth.messages()[i].media;
        if (!media.spoiler && !media.secret && top<conversation_bottom && top+conversation.bubbles[i].height>conversation_top) {
            const auto path=auth.media_file(media.preview_id).path;
            if (!path.empty()) visible.push_back(path);
        }
    }
    vita2d_wait_rendering_done();
    for (const auto& path : visible) {
        if (std::any_of(media_textures.begin(),media_textures.end(),[&](const AvatarTexture& t) { return t.path==path; })) continue;
        if (media_textures.size()>=media_texture_limit) {
            const auto victim=std::find_if(media_textures.begin(),media_textures.end(),[&](const AvatarTexture& t) { return std::find(visible.begin(),visible.end(),t.path)==visible.end(); });
            if (victim==media_textures.end()) break;
            if (victim->texture) vita2d_free_texture(victim->texture);
            media_textures.erase(victim);
        }
        media_textures.push_back({path,load_media_image(path)});
    }
}
void rounded(float x, float y, float width, float height, unsigned int color) {
    constexpr float radius = corner_radius;
    vita2d_draw_rectangle(x+radius,y,width-radius*2,height,color);
    vita2d_draw_rectangle(x,y+radius,width,height-radius*2,color);
    for (float cx : {x+radius,x+width-radius})
        for (float cy : {y+radius,y+height-radius}) vita2d_draw_fill_circle(cx,cy,radius,color);
}
std::string timestamp(std::int64_t date) {
    if (!date) return "";
    const std::time_t value = static_cast<std::time_t>(date);
    const auto* time = std::localtime(&value);
    char buffer[32]{};
    if (time) std::strftime(buffer,sizeof(buffer),timestamp_format,time);
    return buffer;
}
void avatar(vita2d_pgf* font, int x, int y, const std::string& name,int radius = avatar_radius) {
    vita2d_draw_fill_circle(x,y,radius,avatar_color);
    std::size_t end = 1;
    while (end < name.size() && (static_cast<unsigned char>(name[end]) & 0xc0) == 0x80) ++end;
    const auto initial = name.empty() ? "?" : name.substr(0,end);
    text(font,x-7,y+7,initial.c_str(),white,0.8f);
}
void bubbles(vita2d_pgf* font, const Navigation& nav, const telegram::Auth& auth, const Conversation& conversation) {
    const auto& messages = auth.messages();
    vita2d_enable_clipping(); vita2d_set_clip_rectangle(content_x-bubble_focus_border,conversation_top,
        screen_width-28,conversation_bottom);
    for (std::size_t i=0;i<messages.size();++i) {
        const auto& message=messages[i]; const auto& layout=conversation.bubbles[i];
        const float y=ui::bubble_top(layout,nav.scroll);
        if (y>=conversation_bottom || y+layout.height<=conversation_top) continue;
        const int x=message.outgoing ? screen_width-24-message_avatar_size-message_avatar_gap-bubble_width : content_x+message_avatar_size+message_avatar_gap;
        const int avatar_x=message.outgoing ? screen_width-24-message_avatar_size/2 : content_x+message_avatar_size/2;
        const auto path=auth.avatar_path(message);
        const auto photo=std::find_if(avatar_textures.begin(),avatar_textures.end(),[&](const AvatarTexture& avatar) { return avatar.path==path; });
        if (photo!=avatar_textures.end() && photo->texture)
            vita2d_draw_texture_scale(photo->texture,avatar_x-message_avatar_size/2,y,
                static_cast<float>(message_avatar_size)/vita2d_texture_get_width(photo->texture),
                static_cast<float>(message_avatar_size)/vita2d_texture_get_height(photo->texture));
        else avatar(font,avatar_x,static_cast<int>(y)+message_avatar_size/2,auth.sender_name(message),message_avatar_size/2);
        if (i==nav.row) rounded(x-bubble_focus_border,y-bubble_focus_border,
            bubble_width+2*bubble_focus_border,layout.height+2*bubble_focus_border,accent);
        rounded(x,y,bubble_width,layout.height,message.outgoing ? outgoing : panel);
        int baseline=static_cast<int>(y)+bubble_text_baseline;
        for (const auto& line : layout.title_lines) {
            if (baseline>=conversation_top && baseline<=conversation_bottom+sender_line_height)
                text(font,x+bubble_padding,baseline,line.c_str(),accent,message_scale);
            baseline+=sender_line_height;
        }
        for (const auto& line : layout.forward_lines) {
            if (baseline>=conversation_top && baseline<=conversation_bottom+sender_line_height)
                text(font,x+bubble_padding,baseline,line.c_str(),muted,message_scale);
            baseline+=sender_line_height;
        }
        if (message.media.kind!=telegram::MediaKind::None) {
            const auto& media=message.media;
            const auto file=auth.media_file(media.preview_id);
            const auto image=std::find_if(media_textures.begin(),media_textures.end(),[&](const AvatarTexture& t) { return t.path==file.path; });
            const float media_y=baseline-18;
            vita2d_draw_rectangle(x+bubble_padding,media_y,bubble_width-2*bubble_padding,media_preview_height,background);
            if (!media.spoiler && !media.secret && image!=media_textures.end() && image->texture)
                fit_media(image->texture,x+bubble_padding,media_y,bubble_width-2*bubble_padding,media_preview_height);
            else {
                const std::string label=media.secret ? "Self-destructing media unsupported" : media.spoiler ? "Spoiler - Cross to reveal" : file.failed ? "Preview unavailable" : !file.path.empty() ? "Preview format unsupported" : media.preview_id ? "Loading preview..." : media.label;
                clipped(font,x+bubble_padding+10,static_cast<int>(media_y)+90,label,muted,0.75f,bubble_width-2*bubble_padding-20);
            }
            baseline+=media_preview_height;
            text(font,x+bubble_padding,baseline,(media.label+" · Cross: fullscreen").c_str(),accent,0.75f);
            baseline+=media_label_height;
        }
        for (const auto& line : layout.lines) {
            if (baseline>=conversation_top && baseline<=conversation_bottom+bubble_line_height)
                text(font,x+bubble_padding,baseline,line.c_str(),white,message_scale);
            baseline+=bubble_line_height;
        }
        text(font,x+bubble_padding,static_cast<int>(y+layout.height)-bubble_padding,
            timestamp(message.date).c_str(),muted,timestamp_scale);
    }
    vita2d_disable_clipping();
    if (!messages.empty()) {
        const float maximum=ui::maximum_scroll(conversation.height);
        const float progress=maximum>0 ? 1-nav.scroll/maximum : 1;
        vita2d_draw_rectangle(screen_width-22,conversation_top,3,conversation_bottom-conversation_top,panel);
        vita2d_draw_rectangle(screen_width-23,conversation_top+progress*(conversation_bottom-conversation_top-30),5,30,accent);
    }
    clipped(font,content_x,470,auth.history_status(),muted,0.8f,content_width);
}
void draw(vita2d_pgf* font, vita2d_texture* icon, const Navigation& nav, const telegram::Auth& auth,
          const LoginForm& form, const std::string& local_error, bool ime, const Conversation& conversation,const MediaViewer& viewer,const update::State& update) {
    using telegram::AuthStage;
    if (viewer.active()) {
        vita2d_start_drawing(); vita2d_clear_screen(); viewer.draw(font);
        vita2d_end_drawing(); vita2d_swap_buffers(); return;
    }
    const bool ready = auth.stage() == AuthStage::Ready;
    const auto selected = nav.selected;
    prepare_avatars(nav,auth,conversation);
    prepare_media(nav,auth,conversation);
    vita2d_start_drawing(); vita2d_clear_screen();
    vita2d_draw_rectangle(0,0,screen_width,header_height,panel);
    if (icon) vita2d_draw_texture_scale(icon,24,14,0.34f,0.34f);
    text(font,82,46,"Vita TG",white,1.35f);
    text(font,730,43,auth.stage()==AuthStage::Ready ? "Signed in" : "Not signed in",
         auth.stage()==AuthStage::Ready ? success : muted);
    vita2d_draw_rectangle(0,header_height,sidebar_width,footer_top-header_height,panel);
    text(font,24,110,"Telegram client",muted);
    for (std::size_t i=0;i<(ready ? items.size() : 1);++i) {
        int top=sidebar_top+static_cast<int>(i)*sidebar_step;
        if (i==selected) {
            vita2d_draw_rectangle(12,top,276,sidebar_row_height,focus);
            vita2d_draw_rectangle(12,top,4,sidebar_row_height,accent);
        }
        text(font,28,top+36,ready ? items[i] : "Sign in");
    }
    if (ready) clipped(font,24,427,auth.account_name(),muted,0.8f,260);
    text(font,24,456,auth.test_dc() ? "TEST SERVERS" : "Telegram production",muted,0.75f);
    clipped(font,content_x,122,ready ? (nav.history ? auth.history_title().c_str() : nav.updates ? "App updates" : nav.about ? "About Vita TG" : items[selected]) : "Sign in",white,1.1f,(content_width-10));
    if (!ready) {
            const char* steps[]={"1  Phone","2  Code","3  2FA"};
            for (int i=0;i<3;++i) {
                unsigned int color=form.step()==i ? accent : panel;
                vita2d_draw_rectangle(content_x+i*200,143,190,36,color);
                text(font,344+i*200,168,steps[i],form.step()==i ? background : muted,0.8f);
            }
            clipped(font,content_x,216,auth.status(),white,1.0f,(content_width-10));
            paragraph(font,249,local_error.empty() ? auth.detail() : local_error,
                      local_error.empty() ? muted : error_color,2);
            if (form.password() && local_error.empty() && !auth.password_hint().empty())
                clipped(font,content_x,279,"Hint: " + auth.password_hint(),muted,0.8f,(content_width-10));
            if (auth.has_input_stage()) {
                text(font,content_x,309,form.label(),muted,0.85f);
                text(font,770,309,"Cross: edit",accent,0.8f);
                vita2d_draw_rectangle(content_x,322,content_width,55,accent);
                vita2d_draw_rectangle(332,324,586,list_row_height,panel);
                clipped(font,347,357,form.display(),form.value().empty() ? muted : white,1.0f,551);
                bool enabled=!form.value().empty() && !auth.busy();
                vita2d_draw_rectangle(content_x,396,content_width,48,enabled ? accent : panel);
                text(font,350,427,auth.busy() ? "Waiting for Telegram..." : (std::string("Square: ")+form.action()).c_str(),enabled ? background : muted,0.95f);
                if (auth.stage()==AuthStage::Code) {
                    std::string resend=auth.can_resend() ? "Triangle: resend code" : auth.resend_seconds()>0 ?
                        "Resend available in " + std::to_string(auth.resend_seconds()) + " seconds" : "Waiting for Telegram's code delivery";
                    text(font,content_x,473,resend.c_str(),muted,0.8f);
                } else if (form.password()) text(font,content_x,473,"Password stays hidden and is never saved.",muted,0.8f);
                else text(font,content_x,473,"Circle: clear entry",muted,0.8f);
            } else if (auth.stage()==AuthStage::MissingConfig || auth.stage()==AuthStage::Failed || auth.stage()==AuthStage::Closed) {
                text(font,content_x,390,"Cross: load configuration and connect",accent,0.9f);
            }
    } else if (nav.history) {
        bubbles(font,nav,auth,conversation);
    } else if (nav.updates) {
        text(font,content_x,183,"Installed: " VITA_TG_VERSION,muted);
        paragraph(font,225,update.status,white,2);
        paragraph(font,300,update.detail,muted,4);
        if (update.busy) text(font,content_x,410,(std::to_string(update.percent)+"%   Circle: cancel").c_str(),accent,0.9f);
        else text(font,content_x,440,"Cross: check / download   Circle: back",accent,0.9f);
    } else if (nav.about) {
        paragraph(font,183,"An independent client using the Telegram API.",white);
        text(font,content_x,292,"Vita TG " VITA_TG_VERSION,muted);
        text(font,content_x,335,"Powered by native TDLib",muted);
        text(font,content_x,370,"Emoji: Twemoji (CC BY 4.0)",muted,0.8f);
        text(font,content_x,410,"Circle: back to Settings",accent,0.9f);
    } else {
        std::vector<std::string> rows;
        std::string status;
        if (selected == 0) {
            for (const auto& chat : auth.chats()) rows.push_back(chat.title);
            status = auth.chats_status();
            if (rows.empty() && status.empty()) status = "No chats loaded. Square: load chats.";
        } else if (selected == 1) {
            for (const auto& contact : auth.contacts()) rows.push_back(contact.name);
            status = auth.contacts_status();
        } else if (selected == 2) status = "Cross: open Saved Messages";
        else { rows = {"About Vita TG", "App updates", auth.busy() ? "Logging out..." : "Log out"}; status = auth.busy() ? "Waiting for Telegram" : auth.status() == "Telegram rejected the request" ? auth.detail() : "Cross: select   Circle: sidebar"; }
        const auto first = nav.row / list_page_size * list_page_size;
        for (std::size_t i = first; i < rows.size() && i < first + list_page_size; ++i) {
            const int y = list_top + static_cast<int>(i-first)*list_step;
            if (nav.content && i == nav.row) vita2d_draw_rectangle(322,y,606,list_row_height,focus);
            if (selected == 0 || selected == 1) {
                avatar(font,354,y+26,rows[i]);
                clipped(font,388,y+23,rows[i],white,0.9f,518);
                if (selected == 0) clipped(font,388,y+44,auth.chats()[i].preview,muted,0.7f,518);
                else text(font,388,y+44,"Telegram contact",muted,0.7f);
            } else clipped(font,338,y+33,rows[i],white,0.95f,570);
        }
        clipped(font,content_x,470,status,muted,0.8f,content_width);
    }
    vita2d_draw_rectangle(0,footer_top,screen_width,screen_height-footer_top,panel);
    text(font,28,523,ready ? (nav.history ? "Stick: scroll   Cross: media   Circle: back   Square: older" : nav.content ? "D-pad: scroll   Cross: open   Circle: back   Square: load   Touch: swipe" : "Up / Down: page   Cross: open   Square: refresh   Start: exit") : "Cross: edit   Square: submit   Circle: clear   Triangle: resend   Start: exit",muted,0.9f);
    vita2d_end_drawing();
    if (ime) vita2d_common_dialog_update();
    vita2d_swap_buffers();
}

}
