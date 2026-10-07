#include "ui/navigation.hpp"
#include <cassert>
#include <initializer_list>

int main() {
    ui::Navigation saved;
    saved.selected = 2;
    saved.content = saved.history = true;
    saved.row = 3;
    saved.scroll = 240;
    assert(!saved.back());
    assert(!saved.content && saved.history && saved.selected == 2);
    assert(saved.row == 3 && saved.scroll == 240);
    assert(!saved.back());
    assert(!saved.content && saved.history && saved.scroll == 240);

    for (std::size_t page : {0u, 1u}) {
        ui::Navigation chat;
        chat.selected = page;
        chat.content = chat.history = true;
        chat.scroll = 240;
        assert(chat.back());
        assert(chat.content && !chat.history && chat.scroll == 0);
        assert(!chat.back());
        assert(!chat.content);
    }
    ui::Navigation about;
    about.selected = 3;
    about.content = about.about = true;
    assert(!about.back());
    assert(about.content && !about.about);
    assert(!about.back());
    assert(!about.content);
}
