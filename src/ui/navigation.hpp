#pragma once
#include <cstddef>

namespace ui {
struct Navigation {
    std::size_t selected = 0, row = 0;
    bool content = false, history = false, about = false, updates = false;
    float scroll = 0;

    // Saved Messages is the sidebar page itself, rather than a nested chat.
    // Return true only when the caller should release the open conversation.
    bool back() {
        if (history && selected == 2) {
            content = false;
            return false;
        }
        if (history || about || updates) {
            const bool leave = history;
            history = about = updates = false;
            row = 0;
            scroll = 0;
            return leave;
        }
        content = false;
        row = 0;
        return false;
    }
};
}
