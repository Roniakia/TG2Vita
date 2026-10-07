#pragma once
#include <array>
#include <cstdint>
#include <string>

class TextEntry {
public:
    bool open(const char16_t* title, bool password);
    // Returns true once the dialog finished; accepted distinguishes cancel.
    bool finish(std::string& text, bool& accepted);
    bool active() const { return active_; }
    void cancel();
private:
    void clear();
    bool active_ = false;
    std::array<std::uint16_t, 257> buffer_{};
    std::array<std::uint16_t, 2> initial_{};
};
