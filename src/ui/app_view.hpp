#pragma once
#include <vita2d.h>
#include "update/updater.hpp"
#include "theme.hpp"
#include "media_viewer.hpp"
#include "navigation.hpp"
#include "conversation_layout.hpp"
#include "login_form.hpp"
#include "telegram/auth.hpp"

namespace ui {
struct Conversation {
    std::vector<telegram::Message> source;
    std::vector<BubbleLayout> bubbles;
    float height = 0;
    std::vector<std::string> titles, forwards;
    void sync(vita2d_pgf* font,const telegram::Auth& auth);
};
void clear_avatars();
void clear_media();
void init_emoji();
void clear_emoji();
void draw(vita2d_pgf* font,vita2d_texture* icon,const Navigation& nav,const telegram::Auth& auth,
    const LoginForm& form,const std::string& local_error,bool ime,const Conversation& conversation,const MediaViewer& viewer,const update::State& update);
}
