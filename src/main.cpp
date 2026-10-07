#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/apputil.h>
#include <psp2/common_dialog.h>
#include <psp2/io/stat.h>
#include <vita2d.h>
#include "telegram/auth.hpp"
#include "telegram/ime.hpp"
#include "ui/login_form.hpp"
#include "ui/theme.hpp"
#include "ui/app_view.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>

// TDLib parsing and encrypted session management need a larger heap than the shell.
extern "C" { unsigned int _newlib_heap_size_user = 128 * 1024 * 1024; }
extern "C" const unsigned int sceUserMainThreadStackSize = 512 * 1024;

namespace {
using namespace ui::theme;
using ui::Navigation;
using ui::Conversation;
class Network {
public:
    bool init() {
        if (ready_) return true;
        if (attempted_) return false;
        attempted_ = true;
        if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0) return false;
        module_ = true;
        memory_ = std::malloc(4 * 1024 * 1024);
        if (!memory_) return false;
        SceNetInitParam param{};
        param.memory = memory_; param.size = 4 * 1024 * 1024;
        if (sceNetInit(&param) < 0) return false;
        net_ = true;
        if (sceNetCtlInit() < 0) return false;
        ready_ = true;
        return true;
    }
    ~Network() {
        if (ready_) sceNetCtlTerm();
        if (net_) sceNetTerm();
        std::free(memory_);
        if (module_) sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    }
private:
    void* memory_ = nullptr;
    bool module_ = false, net_ = false, ready_ = false, attempted_ = false;
};
}

int main() {
    if (vita2d_init() < 0) { sceKernelExitProcess(1); return 1; }
    vita2d_set_clear_color(background);
    vita2d_pgf* font = vita2d_load_default_pgf();
    if (!font) { vita2d_fini(); sceKernelExitProcess(1); return 1; }
    ui::init_emoji();
    vita2d_texture* icon = vita2d_load_PNG_file("app0:sce_sys/icon0.png");
    SceAppUtilInitParam app_param{};
    SceAppUtilBootParam boot_param{};
    sceAppUtilInit(&app_param, &boot_param);
    SceCommonDialogConfigParam dialog_config{};
    sceCommonDialogSetConfigParam(&dialog_config);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);
    SceTouchPanelInfo touch_panel{};
    const bool touch_available = sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT,&touch_panel) >= 0 &&
        touch_panel.maxAaX > touch_panel.minAaX && touch_panel.maxAaY > touch_panel.minAaY;
    sceIoMkdir("ux0:data/vita-tg", 0700);
    sceIoMkdir("ux0:data/vita-tg/session", 0700);
    sceIoMkdir("ux0:data/vita-tg/session-test", 0700);
    sceIoMkdir("ux0:data/vita-tg/files-test", 0700);
    sceIoMkdir("ux0:data/vita-tg/files", 0700);
    {
        // Network outlives TDLib's workers, including during shutdown.
        Network network;
        auto auth = std::make_unique<telegram::Auth>();
        TextEntry entry;
        LoginForm form;
        telegram::AuthStage entry_stage = telegram::AuthStage::MissingConfig;
        Navigation nav;
        Conversation conversation;
        ui::MediaViewer viewer;
        update::Updater updater;
        std::uint32_t previous = 0;
        std::string local_error;
        bool exiting = false;
        bool touching = false, dragged = false;
        int touch_x = 0, touch_y = 0, drag_y = 0;
        auto connect = [&] {
            local_error.clear();
            telegram::Credentials credentials;
            if (!telegram::load_application_credentials(credentials, local_error)) return;
            if (!network.init()) { local_error = "Network initialization failed; restart the app"; return; }
            auth->start(credentials);
            std::fill(credentials.api_hash.begin(), credentials.api_hash.end(), '\0');
        };
        auto select_page = [&](std::size_t selected) {
            if (nav.history) auth->leave_chat();
            nav = {}; nav.selected = selected;
            if (selected==0) auth->refresh_chats();
            else if (selected==1) auth->refresh_contacts();
            else if (selected==2) { nav.history = true; auth->open_saved_messages(); }
        };
        if (network.init()) updater.check();
        connect();
        auto previous_frame=std::chrono::steady_clock::now();
        while (!exiting) {
            const auto frame_time=std::chrono::steady_clock::now();
            const float frame_seconds=std::min(0.05f,std::chrono::duration<float>(frame_time-previous_frame).count());
            previous_frame=frame_time;
            auto previous_stage=auth->stage();
            const auto focused_id=nav.history && nav.scroll>0 && nav.row<auth->messages().size() ? auth->messages()[nav.row].id : 0;
            const float old_offset=focused_id && nav.row<conversation.bubbles.size() ? conversation.bubbles[nav.row].bottom_offset : 0;
            auth->poll();
            conversation.sync(font,*auth);
            if (focused_id) {
                const auto& messages=auth->messages();
                const auto found=std::find_if(messages.begin(),messages.end(),[focused_id](const telegram::Message& message) { return message.id==focused_id; });
                if (found!=messages.end()) {
                    nav.row=static_cast<std::size_t>(found-messages.begin());
                    nav.scroll+=conversation.bubbles[nav.row].bottom_offset-old_offset;
                }
            }
            nav.scroll=std::max(0.0f,std::min(nav.scroll,ui::maximum_scroll(conversation.height)));
            viewer.update(*auth);
            form.sync(auth->stage());
            if (auth->stage()!=previous_stage) {
                local_error.clear(); nav = {};
            }
            if (entry.active() && auth->stage() != entry_stage) entry.cancel();
            std::string input;
            bool accepted = false;
            if (entry.finish(input, accepted)) {
                if (accepted && auth->stage() == entry_stage) { form.set(std::move(input)); local_error.clear(); }
                volatile char* p = input.empty() ? nullptr : &input[0];
                for (std::size_t i = 0; i < input.size(); ++i) p[i] = 0;
                input.clear();
            }
            SceCtrlData pad{};
            if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0) {
                auto pressed = pad.buttons & ~previous;
                previous = pad.buttons;
                if (viewer.active()) {
                    if (pressed&SCE_CTRL_START) { viewer.close(); exiting=true; }
                    else {
                        viewer.controls(pressed);
                    }
                } else if (!entry.active()) {
                    SceTouchData touch{};
                    if (touch_available && sceTouchPeek(SCE_TOUCH_PORT_FRONT,&touch,1)>0) {
                        if (touch.reportNum && auth->stage()==telegram::AuthStage::Ready) {
                            const int x = (touch.report[0].x-touch_panel.minAaX)*screen_width/(touch_panel.maxAaX-touch_panel.minAaX);
                            const int y = (touch.report[0].y-touch_panel.minAaY)*screen_height/(touch_panel.maxAaY-touch_panel.minAaY);
                            if (!touching) { touching = true; dragged = false; touch_x = x; touch_y = drag_y = y; }
                            else if (std::abs(x-touch_x)>touch_horizontal_slop) dragged = true;
                            else if (touch_x >= sidebar_width && touch_y >= conversation_top && touch_y < conversation_bottom && std::abs(y-drag_y)>=(nav.history ? (dragged ? touch_drag_step : touch_slop) : list_swipe_step)) {
                                dragged = true; nav.content = true;
                                // Conversation order is oldest at the top, newest at the bottom.
                                if (nav.history) {
                                    const float maximum=ui::maximum_scroll(conversation.height);
                                    nav.scroll = std::max(0.0f,std::min(maximum,nav.scroll+y-drag_y));
                                    const auto row=ui::hit_bubble(conversation.bubbles,nav.scroll,(conversation_top+conversation_bottom)/2);
                                    if (row<conversation.bubbles.size()) nav.row=row;
                                } else pressed |= (y > drag_y) ? SCE_CTRL_UP : SCE_CTRL_DOWN;
                                drag_y = y;
                            }
                        } else if (touching) {
                            touching = false;
                            if (!dragged && auth->stage()==telegram::AuthStage::Ready) {
                                if (touch_x < sidebar_width && touch_y >= sidebar_top && touch_y < sidebar_top+static_cast<int>(items.size())*sidebar_step) {
                                    select_page(static_cast<std::size_t>((touch_y-sidebar_top)/sidebar_step));
                                    pressed |= SCE_CTRL_CROSS;
                                } else if (touch_x >= sidebar_width && touch_y >= conversation_top && touch_y < conversation_bottom && !nav.about && !nav.updates) {
                                    nav.content = true;
                                    if (nav.history) {
                                        const auto row=ui::hit_bubble(conversation.bubbles,nav.scroll,touch_y);
                                        if (row<auth->messages().size()) { nav.row=row; pressed |= SCE_CTRL_CROSS; }
                                    } else {
                                        nav.row = nav.row/list_page_size*list_page_size+static_cast<std::size_t>((touch_y-list_top)/list_step);
                                        pressed |= SCE_CTRL_CROSS;
                                    }
                                } else if (touch_x >= sidebar_width && touch_y >= conversation_bottom && touch_y < footer_top) pressed |= SCE_CTRL_SQUARE;
                                else if (touch_x >= sidebar_width && touch_y >= header_height && touch_y < conversation_top) pressed |= SCE_CTRL_CIRCLE;
                            }
                        }
                    }
                    if (pressed & SCE_CTRL_START) exiting = true;
                    const bool ready = auth->stage() == telegram::AuthStage::Ready;
                    if (ready) {
                        if (nav.history) {
                            nav.scroll=std::max(0.0f,std::min(nav.scroll+ui::stick_scroll(pad.ry,frame_seconds),ui::maximum_scroll(conversation.height)));
                        }
                        const auto& chats = auth->chats(); const auto& contacts = auth->contacts();
                        const auto count = nav.history ? auth->messages().size() : nav.selected == 0 ? chats.size() : nav.selected == 1 ? contacts.size() : nav.selected == 3 ? 3u : 0u;
                        if (nav.content) {
                            if (nav.row >= count) nav.row = count ? count-1 : 0;
                            if (nav.history) {
                                if (pressed & SCE_CTRL_UP) nav.scroll+=scroll_step;
                                if (pressed & SCE_CTRL_DOWN) nav.scroll-=scroll_step;
                                nav.scroll=std::max(0.0f,std::min(nav.scroll,ui::maximum_scroll(conversation.height)));
                                if ((pressed&(SCE_CTRL_UP|SCE_CTRL_DOWN)) || ui::stick_scroll(pad.ry,frame_seconds)!=0) {
                                    const auto row=ui::hit_bubble(conversation.bubbles,nav.scroll,(conversation_top+conversation_bottom)/2);
                                    if (row<count) nav.row=row;
                                }
                            } else {
                                if ((pressed & SCE_CTRL_UP) && nav.row > 0) --nav.row;
                                if ((pressed & SCE_CTRL_DOWN) && nav.row+1 < count) ++nav.row;
                            }
                        } else {
                            if ((pressed & SCE_CTRL_UP) && nav.selected > 0) select_page(nav.selected-1);
                            if ((pressed & SCE_CTRL_DOWN) && nav.selected+1 < items.size()) select_page(nav.selected+1);
                        }
                        if (pressed & SCE_CTRL_CIRCLE) {
                            if (nav.updates && updater.state().busy) updater.cancel();
                            else if (nav.back()) auth->leave_chat();
                        }
                        if (pressed & SCE_CTRL_CROSS) {
                            if (!nav.content) {
                                nav.content = true; nav.row = 0;
                                if (nav.selected == 2 && !nav.history) { nav.history = true; auth->open_saved_messages(); }
                            } else if (nav.updates) {
                                if (updater.state().available) updater.download(); else updater.check();
                            } else if (nav.history && nav.row<auth->messages().size()) {
                                viewer.open(auth->messages()[nav.row],*auth);
                            } else if (!nav.history && !nav.about && !nav.updates) {
                                if (nav.selected == 0 && nav.row < chats.size()) { auth->open_chat(chats[nav.row].id); nav.history = true; nav.row = 0; nav.scroll = 0; }
                                else if (nav.selected == 1 && nav.row < contacts.size()) { auth->open_contact(contacts[nav.row].id); nav.history = true; nav.row = 0; nav.scroll = 0; }
                                else if (nav.selected == 2) { auth->open_saved_messages(); nav.history = true; }
                                else if (nav.selected == 3) { if (nav.row == 0) nav.about = true; else if (nav.row == 1) nav.updates = true; else auth->logout(); }
                            }
                        }
                        if (pressed & SCE_CTRL_SQUARE) {
                            if (nav.history) auth->more_history();
                            else if (nav.selected == 0) auth->refresh_chats();
                            else if (nav.selected == 1) auth->refresh_contacts();
                        }
                    }
                    if (ready && (pressed & SCE_CTRL_TRIANGLE) && updater.state().available) {
                        select_page(3); nav.content=true; nav.row=1; nav.updates=true;
                    }
                    if (auth->stage() != telegram::AuthStage::Ready && (pressed & SCE_CTRL_CROSS)) {
                        if (auth->stage() == telegram::AuthStage::MissingConfig || auth->stage() == telegram::AuthStage::Failed || auth->stage() == telegram::AuthStage::Closed) {
                            auth.reset();
                            auth = std::make_unique<telegram::Auth>();
                            connect();
                        }
                        else if (auth->needs_input()) {
                            const auto stage = auth->stage();
                            entry_stage = stage;
                            if (!entry.open(form.title(), form.password())) local_error = "Unable to open text input";
                            else local_error.clear();
                        }
                    }
                    if (auth->stage() != telegram::AuthStage::Ready && (pressed & SCE_CTRL_SQUARE) && auth->needs_input()) {
                        if (form.validate(local_error)) { auth->submit(form.value()); form.clear(); }
                    }
                    if (auth->stage() != telegram::AuthStage::Ready && (pressed & SCE_CTRL_CIRCLE)) { form.clear(); local_error.clear(); }
                    if (auth->stage() != telegram::AuthStage::Ready && (pressed & SCE_CTRL_TRIANGLE)) {
                        auth->resend_code();
                    }
                }
            }
            if (nav.history && !viewer.active()) for (std::size_t i=0;i<conversation.bubbles.size();++i) {
                const auto top=ui::bubble_top(conversation.bubbles[i],nav.scroll);
                if (top<conversation_bottom && top+conversation.bubbles[i].height>conversation_top) { auth->request_avatar(auth->messages()[i]); auth->request_media(auth->messages()[i]); }
            }
            if (nav.history && !viewer.active() && ui::near_history_edge(conversation.height,nav.scroll)) auth->prefetch_history();
            ui::draw(font, icon, nav, *auth, form, local_error, entry.active(), conversation,viewer,updater.state());
            sceDisplayWaitVblankStart();
        }
        updater.cancel();
        viewer.close();
        entry.cancel();
        auth->close();
        // Destroy joins client workers; the network stays initialized until then.
        auth.reset();
    }
    vita2d_wait_rendering_done();
    ui::clear_avatars();
    ui::clear_media();
    ui::clear_emoji();
    if (icon) vita2d_free_texture(icon);
    vita2d_free_pgf(font);
    sceAppUtilShutdown();
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
