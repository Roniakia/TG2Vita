#include "ime.hpp"
#include <psp2/ime_dialog.h>

namespace {
void append_utf8(std::string& out, std::uint32_t c) {
    if (c <= 0x7f) out += static_cast<char>(c);
    else if (c <= 0x7ff) { out += static_cast<char>(0xc0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 63)); }
    else if (c <= 0xffff) { out += static_cast<char>(0xe0 | (c >> 12)); out += static_cast<char>(0x80 | ((c >> 6) & 63)); out += static_cast<char>(0x80 | (c & 63)); }
    else { out += static_cast<char>(0xf0 | (c >> 18)); out += static_cast<char>(0x80 | ((c >> 12) & 63)); out += static_cast<char>(0x80 | ((c >> 6) & 63)); out += static_cast<char>(0x80 | (c & 63)); }
}
}
void TextEntry::clear() {
    volatile std::uint16_t* p = buffer_.data();
    for (std::size_t i = 0; i < buffer_.size(); ++i) p[i] = 0;
}
bool TextEntry::open(const char16_t* title, bool password) {
    if (active_) return false;
    clear();
    SceImeDialogParam param;
    sceImeDialogParamInit(&param);
    param.supportedLanguages = SCE_IME_LANGUAGE_ENGLISH;
    param.languagesForced = SCE_FALSE;
    param.type = SCE_IME_TYPE_DEFAULT;
    param.textBoxMode = password ? SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD : SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
    param.title = reinterpret_cast<const SceWChar16*>(title);
    param.maxTextLength = 256;
    param.initialText = initial_.data();
    param.inputTextBuffer = buffer_.data();
    active_ = sceImeDialogInit(&param) >= 0;
    return active_;
}
bool TextEntry::finish(std::string& text, bool& accepted) {
    if (!active_ || sceImeDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) return false;
    SceImeDialogResult result{};
    const int rc = sceImeDialogGetResult(&result);
    accepted = rc >= 0 && result.result == 0 && result.button == SCE_IME_DIALOG_BUTTON_ENTER;
    text.clear();
    if (accepted) {
        for (std::size_t i = 0; i < buffer_.size() && buffer_[i]; ++i) {
            std::uint32_t c = buffer_[i];
            if (c >= 0xd800 && c <= 0xdbff) {
                if (i + 1 < buffer_.size() && buffer_[i+1] >= 0xdc00 && buffer_[i+1] <= 0xdfff)
                    c = 0x10000 + ((c - 0xd800) << 10) + (buffer_[++i] - 0xdc00);
                else c = 0xfffd;
            } else if (c >= 0xdc00 && c <= 0xdfff) c = 0xfffd;
            append_utf8(text, c);
        }
    }
    sceImeDialogTerm();
    active_ = false;
    clear();
    return true;
}
void TextEntry::cancel() {
    if (active_) { sceImeDialogAbort(); sceImeDialogTerm(); active_ = false; }
    clear();
}
