#include "version.hpp"
#include "auth.hpp"
#include "client_config.hpp"
#include <td/telegram/td_json_client.h>
#include <jansson.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/rng.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>

namespace telegram {
namespace {
const char* string_field(json_t* object, const char* key) {
    const char* s = json_string_value(json_object_get(object, key));
    return s ? s : "";
}
void wipe(std::string& s) {
    volatile char* data = s.empty() ? nullptr : &s[0];
    for (std::size_t i = 0; i < s.size(); ++i) data[i] = 0;
    s.clear();
}
void dispatch(void* client, json_t* request) {
    char* encoded = json_dumps(request, JSON_COMPACT);
    if (encoded) {
        td_json_client_send(client, encoded);
        volatile char* p = encoded;
        for (std::size_t i = 0; encoded[i]; ++i) p[i] = 0;
        free(encoded);
    }
    json_decref(request);
}
}



namespace {
std::int64_t number(json_t* value, const char* key) {
    auto field=json_object_get(value,key);
    if (json_is_integer(field)) return json_integer_value(field);
    // TDLib JSON encodes int64 (notably chat order) as decimal strings.
    const auto text=json_string_value(field);
    if (!text || !*text) return 0;
    char* end=nullptr; errno=0;
    const auto result=std::strtoll(text,&end,10);
    return errno==0 && end && !*end ? result : 0;
}
std::string message_text(json_t* message) {
    if (!json_is_object(message)) return "";
    auto content = json_object_get(message, "content");
    const std::string type = string_field(content, "@type");
    if (type == "messageText") return std::string(string_field(json_object_get(content, "text"), "text"));
    const std::string caption = string_field(json_object_get(content, "caption"), "text");
    return caption.empty() ? "[Media or service message]" : "[Media] " + caption;
}
void position(Chat& chat, json_t* value) {
    if (std::strcmp(string_field(json_object_get(value, "list"), "@type"), "chatListMain") == 0)
        chat.order = number(value, "order");
}
}
void Auth::resolve_sender(std::int64_t id,bool chat) {
    if (!id) return;
    if (chat ? chats_.count(id)>0 : users_.count(id)>0) return;
    data_request(json_pack("{s:s,s:I}","@type",chat ? "getChat" : "getUser",chat ? "chat_id" : "user_id",static_cast<json_int_t>(id)),chat ? "sender_chat" : "sender_user",id);
}
std::string Auth::identity_name(std::int64_t id,bool chat) const {
    if (chat) { const auto found=chats_.find(id); if (found!=chats_.end()) return found->second.title; }
    else { const auto found=users_.find(id); if (found!=users_.end()) return found->second.name; }
    if (id==self_id_ && !account_name_.empty()) return account_name_;
    return chat ? "Telegram chat" : "Telegram user";
}
std::string Auth::sender_name(const Message& message) const {
    auto name=identity_name(message.sender_id ? message.sender_id : message.outgoing ? self_id_ : 0,message.sender_chat);
    if (!message.author_signature.empty()) name+=" · "+message.author_signature;
    return name;
}
std::string Auth::forwarding_title(const Message& message) const {
    if (!message.forwarded) return "";
    auto name=message.forward_hidden_name.empty() ? identity_name(message.forward_id,message.forward_chat) : message.forward_hidden_name;
    if (!message.forward_signature.empty()) name+=" · "+message.forward_signature;
    return "Forwarded from "+name;
}
Message Auth::parse_message(json_t* value) {
    Message message;
    message.id=number(value,"id"); message.text=message_text(value);
    parse_media(message,json_object_get(value,"content"));
    message.outgoing=json_is_true(json_object_get(value,"is_outgoing")); message.date=number(value,"date");
    auto sender=json_object_get(value,"sender_id");
    message.sender_chat=std::strcmp(string_field(sender,"@type"),"messageSenderChat")==0;
    message.sender_id=number(sender,message.sender_chat ? "chat_id" : "user_id");
    message.author_signature=string_field(value,"author_signature");
    resolve_sender(message.sender_id,message.sender_chat);
    auto forward=json_object_get(value,"forward_info");
    message.forwarded=json_is_object(forward);
    auto origin=json_object_get(forward,"origin");
    const std::string type=string_field(origin,"@type");
    if (type=="messageOriginUser") message.forward_id=number(origin,"sender_user_id");
    else if (type=="messageOriginHiddenUser") message.forward_hidden_name=string_field(origin,"sender_name");
    else if (type=="messageOriginChat" || type=="messageOriginChannel") {
        message.forward_chat=true;
        message.forward_id=number(origin,type=="messageOriginChat" ? "sender_chat_id" : "chat_id");
        message.forward_signature=string_field(origin,"author_signature");
    }
    resolve_sender(message.forward_id,message.forward_chat);
    return message;
}
std::string Auth::avatar_path(const Message& message) const {
    const auto id=message.sender_id ? message.sender_id : message.outgoing ? self_id_ : 0;
    int photo=0;
    if (message.sender_chat) { const auto user=chats_.find(id); if (user!=chats_.end()) photo=user->second.photo_id; }
    else { const auto user=users_.find(id); if (user!=users_.end()) photo=user->second.photo_id; }
    const auto file=avatar_files_.find(photo); return file==avatar_files_.end() ? "" : file->second;
}
void Auth::request_avatar(const Message& message) {
    const auto id=message.sender_id ? message.sender_id : message.outgoing ? self_id_ : 0;
    int photo=0;
    if (message.sender_chat) { const auto user=chats_.find(id); if (user!=chats_.end()) photo=user->second.photo_id; }
    else { const auto user=users_.find(id); if (user!=users_.end()) photo=user->second.photo_id; }
    if (!photo || avatar_files_.count(photo) || avatar_attempted_.count(photo)) return;
    if (avatar_attempted_.size()>=config::avatar_file_limit) return;
    avatar_attempted_[photo]=true;
    data_request(json_pack("{s:s,s:i,s:i,s:i,s:i,s:b}","@type","downloadFile","file_id",photo,"priority",8,"offset",0,"limit",0,"synchronous",0),"avatar",photo);
}
void Auth::cache_media_file(json_t* file) {
    const int id=static_cast<int>(number(file,"id"));
    auto it=media_files_.find(id); if (it==media_files_.end()) return;
    auto local=json_object_get(file,"local");
    auto& state=it->second;
    state.downloaded=number(local,"downloaded_size");
    state.size=std::max(number(file,"size"),number(file,"expected_size"));
    if (state.limit && (state.size>state.limit || state.downloaded>state.limit)) {
        const bool downloading=state.pending;
        state.path.clear(); state.pending=false; state.failed=true;
        if (downloading) data_request(json_pack("{s:s,s:i,s:b}","@type","cancelDownloadFile","file_id",id,"only_if_pending",0),"media_cancel",id);
        return;
    }
    if (json_is_true(json_object_get(local,"is_downloading_completed"))) {
        state.path=string_field(local,"path"); state.pending=false; state.failed=state.path.empty();
    } else {
        state.path.clear();
        if (json_is_false(json_object_get(local,"is_downloading_active")) && state.pending) { state.pending=false; state.failed=true; }
    }
}
MediaFile Auth::media_file(int id) const {
    const auto it=media_files_.find(id); return it==media_files_.end() ? MediaFile{} : it->second;
}
void Auth::parse_media(Message& message,json_t* content) {
    message.media={}; auto& media=message.media;
    const std::string type=string_field(content,"@type");
    media.spoiler=json_is_true(json_object_get(content,"has_spoiler"));
    media.secret=json_is_true(json_object_get(content,"is_secret"));
    auto assign=[&](json_t* file,bool preview) {
        const int id=static_cast<int>(number(file,"id"));
        const auto size=std::max(number(file,"size"),number(file,"expected_size"));
        if (preview) { media.preview_id=id; media.preview_size=size; }
        else { media.file_id=id; media.file_size=size; }
        if (id && json_is_true(json_object_get(json_object_get(file,"local"),"is_downloading_completed")) && media_files_.size()<config::media_file_limit) media_files_.try_emplace(id);
        cache_media_file(file);
    };
    if (type=="messagePhoto") {
        media.kind=MediaKind::Photo; media.label="Photo";
        json_t* preview=nullptr; json_t* full=nullptr; std::int64_t best=0,small=0;
        size_t i; json_t* size;
        json_array_foreach(json_object_get(json_object_get(content,"photo"),"sizes"),i,size) {
            const auto w=number(size,"width"),h=number(size,"height");
            if (w<=0 || h<=0 || w>2048 || h>2048) continue;
            const auto area=w*h; if (area>2*1024*1024) continue;
            if (!preview || (area>=160*160 && (small<160*160 || area<small)) || (small<160*160 && area>small)) { preview=size; small=area; }
            if (area>best) { full=size; best=area; }
        }
        if (preview) assign(json_object_get(preview,"photo"),true);
        if (full) { assign(json_object_get(full,"photo"),false); media.width=static_cast<int>(number(full,"width")); media.height=static_cast<int>(number(full,"height")); }
    } else if (type=="messageVideo" || type=="messageAnimation" || type=="messageVideoNote") {
        const char* field=type=="messageVideo" ? "video" : type=="messageVideoNote" ? "video_note" : "animation";
        auto value=json_object_get(content,field);
        media.kind=type=="messageAnimation" ? MediaKind::Animation : MediaKind::Video;
        media.label=type=="messageAnimation" ? "Animation" : "Video";
        const std::string mime=string_field(value,"mime_type");
        if (type=="messageAnimation" && mime!="video/mp4") { media.kind=MediaKind::Unsupported; media.label="GIF animation (unsupported)"; }
        media.width=static_cast<int>(number(value,"width")); media.height=static_cast<int>(number(value,"height"));
        assign(json_object_get(value,type=="messageVideoNote" ? "video" : field),false);
        auto thumb=json_object_get(value,"thumbnail");
        const std::string format=string_field(json_object_get(thumb,"format"),"@type");
        if (format=="thumbnailFormatJpeg" || format=="thumbnailFormatPng") assign(json_object_get(thumb,"file"),true);
    } else if (type=="messageDocument" || type=="messageAudio" || type=="messageVoiceNote" || type=="messageSticker" || type=="messagePaidMedia" || type=="messageStory") {
        media.kind=MediaKind::Unsupported;
        media.label=type=="messageDocument" ? "Document" : type=="messageAudio" ? "Audio" : type=="messageVoiceNote" ? "Voice message" : type=="messageSticker" ? "Sticker" : type=="messageStory" ? "Story" : "Paid media";
    }
    if (media.kind!=MediaKind::None && media.kind!=MediaKind::Unsupported) {
        message.text=string_field(json_object_get(content,"caption"),"text");
    }
}
void Auth::cancel_media(int id) {
    auto it=media_files_.find(id);
    if (it==media_files_.end() || !it->second.pending) return;
    it->second.pending=false; it->second.failed=true;
    data_request(json_pack("{s:s,s:i,s:b}","@type","cancelDownloadFile","file_id",id,"only_if_pending",0),"media_cancel",id);
}
void Auth::request_media(const Message& message,bool full,bool retry) {
    const auto& media=message.media;
    if (stage_!=AuthStage::Ready || media.secret || media.kind==MediaKind::None || media.kind==MediaKind::Unsupported || (!full && media.spoiler)) return;
    const int id=full ? media.file_id : media.preview_id;
    const auto size=full ? media.file_size : media.preview_size;
    const auto limit=full ? (media.kind==MediaKind::Photo ? config::media_photo_bytes : config::media_video_bytes) : config::media_preview_bytes;
    if (!id) return;
    auto it=media_files_.find(id);
    if (it==media_files_.end()) {
        if (media_files_.size()>=config::media_file_limit) {
            auto victim=std::find_if(media_files_.begin(),media_files_.end(),[](const auto& entry) { return !entry.second.pending; });
            if (victim==media_files_.end()) {
                if (!full) return;
                victim=media_files_.begin(); cancel_media(victim->first);
            }
            media_files_.erase(victim);
        }
        it=media_files_.emplace(id,MediaFile{}).first;
    }
    auto& state=it->second;
    state.limit=std::max(state.limit,limit);
    if (std::max(size,state.size)>limit || (full && media.kind!=MediaKind::Photo && (media.width>1920 || media.height>1088))) { state.failed=true; return; }
    if (!state.path.empty() || state.pending || (state.failed && !retry)) return;
    state.pending=true; state.failed=false; state.size=size;
    data_request(json_pack("{s:s,s:i,s:i,s:i,s:i,s:b}","@type","downloadFile","file_id",id,"priority",full ? 16 : 4,"offset",0,"limit",static_cast<int>(limit+1),"synchronous",0),"media",id);
}
void Auth::fetch_chat_list() {
    data_request(json_pack("{s:s,s:{s:s},s:i}","@type","getChats","chat_list","@type","chatListMain","limit",config::chat_list_snapshot_limit),"chats");
}
void Auth::clear_data() {
    avatar_files_.clear(); avatar_attempted_.clear(); media_files_.clear();
    data_requests_.clear(); chats_.clear(); users_.clear(); contact_ids_.clear(); messages_.clear();
    chat_rows_.clear(); contact_rows_.clear(); chats_dirty_ = contacts_dirty_ = true;
    self_id_ = history_chat_ = 0; saved_pending_ = history_end_ = initial_history_ = false;
    history_title_.clear(); chats_status_.clear(); contacts_status_.clear(); history_status_.clear();
}
void Auth::data_request(json_t* request, const std::string& kind, std::int64_t id) {
    if (!client_ || stage_ != AuthStage::Ready) { json_decref(request); return; }
    for (const auto& pending : data_requests_)
        if (pending.second == std::make_pair(kind, id)) { json_decref(request); return; }
    json_object_set_new(request, "@extra", json_integer(++serial_));
    data_requests_[serial_] = {kind, id}; dispatch(client_, request);
}
void Auth::refresh_chats() {
    chats_status_ = "Loading chats...";
    data_request(json_pack("{s:s,s:{s:s},s:i}", "@type", "loadChats", "chat_list", "@type", "chatListMain", "limit", config::chat_list_batch), "load");
}
void Auth::refresh_contacts() {
    contacts_status_ = "Loading contacts...";
    data_request(json_pack("{s:s}", "@type", "getContacts"), "contacts");
}
void Auth::open_saved_messages() {
    leave_chat();
    saved_pending_ = true; history_title_ = "Saved Messages"; history_status_ = "Loading Saved Messages...";
    history_chat_ = 0; messages_.clear();
    if (self_id_) { saved_pending_ = false;
        data_request(json_pack("{s:s,s:I,s:b}", "@type", "createPrivateChat", "user_id", static_cast<json_int_t>(self_id_), "force", 1), "saved", history_generation_);
    } else if (!profile_request_) history_status_ = "Account unavailable. Circle: back; try again.";
}
void Auth::open_contact(std::int64_t id) {
    leave_chat();
    history_title_ = users_[id].name; history_status_ = "Opening chat...";
    data_request(json_pack("{s:s,s:I,s:b}", "@type", "createPrivateChat", "user_id", static_cast<json_int_t>(id), "force", 1), "private", history_generation_);
}
void Auth::open_chat(std::int64_t id) {
    leave_chat();
    history_chat_ = id; history_end_ = false; initial_history_ = true; messages_.clear();
    history_title_ = id == self_id_ ? "Saved Messages" : chats_[id].title;
    if (history_title_.empty()) history_title_ = "Chat";
    data_request(json_pack("{s:s,s:I}", "@type", "openChat", "chat_id", static_cast<json_int_t>(id)), "open", id);
    more_history();
}
void Auth::leave_chat() {
    if (history_chat_) data_request(json_pack("{s:s,s:I}", "@type", "closeChat", "chat_id", static_cast<json_int_t>(history_chat_)), "close", history_chat_);
    for (auto it = data_requests_.begin(); it != data_requests_.end();) {
        const auto& kind = it->second.first;
        if (kind == "history" || kind == "saved" || kind == "private") it = data_requests_.erase(it);
        else ++it;
    }
    ++history_generation_; prefetch_cursor_ = 0; initial_history_ = false; history_chat_ = 0; saved_pending_ = false; messages_.clear();
}
void Auth::prefetch_history() {
    if (stage_!=AuthStage::Ready || initial_history_ || history_end_ || messages_.empty()) return;
    for (const auto& request : data_requests_) if (request.second.first=="history") return;
    const auto cursor=messages_.back().id;
    // One automatic attempt per cursor; errors await an explicit Square retry.
    if (cursor==prefetch_cursor_) return;
    prefetch_cursor_=cursor;
    more_history();
}
void Auth::more_history() {
    if (!history_chat_ || history_end_) return;
    history_status_ = "Loading messages...";
    const auto from = messages_.empty() ? 0 : messages_.back().id;
    data_request(json_pack("{s:s,s:I,s:I,s:i,s:i,s:b}", "@type", "getChatHistory",
        "chat_id", static_cast<json_int_t>(history_chat_), "from_message_id", static_cast<json_int_t>(from),
        "offset", 0, "limit", config::history_request_limit, "only_local", 0), "history", history_chat_);
}
const std::vector<Chat>& Auth::chats() const {
    if (!chats_dirty_) return chat_rows_;
    auto& result = chat_rows_; result.clear(); chats_dirty_ = false;
    for (const auto& entry : chats_) if (entry.second.order > 0) result.push_back(entry.second);
    std::sort(result.begin(), result.end(), [](const Chat& a, const Chat& b) { return a.order != b.order ? a.order > b.order : a.id > b.id; });
    return result;
}
const std::vector<Contact>& Auth::contacts() const {
    if (!contacts_dirty_) return contact_rows_;
    auto& result = contact_rows_; result.clear(); contacts_dirty_ = false;
    for (auto id : contact_ids_) { auto it = users_.find(id); if (it != users_.end()) result.push_back(it->second); }
    std::sort(result.begin(), result.end(), [](const Contact& a, const Contact& b) { return a.name < b.name; });
    return result;
}
void Auth::handle_data(json_t* event) {
    if (stage_ != AuthStage::Ready) return;
    const std::string type = string_field(event, "@type");
    auto cache_chat = [&](json_t* value) {
        const auto id = number(value, "id"); if (!id) return;
        if (chats_.size() >= 2000 && !chats_.count(id)) return;
        chats_dirty_ = true;
        Chat& chat = chats_[id]; chat.id = id; chat.title = string_field(value, "title"); chat.photo_id=static_cast<int>(number(json_object_get(json_object_get(value,"photo"),"small"),"id")); chat.preview = message_text(json_object_get(value, "last_message")); chat.order = 0;
        if (json_is_null(json_object_get(value, "last_message")) || !json_object_get(value, "last_message")) chat.preview.clear();
        auto positions = json_object_get(value, "positions"); size_t i; json_t* p;
        json_array_foreach(positions, i, p) position(chat, p);
    };
    auto cache_user = [&](json_t* value) {
        const auto id = number(value, "id"); if (!id) return;
        if (users_.size() >= 2000 && !users_.count(id)) return;
        std::string name = string_field(value, "first_name"), last = string_field(value, "last_name");
        if (!last.empty()) name += " " + last;
        contacts_dirty_ = true;
        users_[id] = {id, name.empty() ? "Telegram user" : name,static_cast<int>(number(json_object_get(json_object_get(value,"profile_photo"),"small"),"id"))};
    };
    auto cache_file=[&](json_t* file) {
        const auto id=static_cast<int>(number(file,"id"));
        cache_media_file(file);
        auto local=json_object_get(file,"local");
        if (id && json_is_true(json_object_get(local,"is_downloading_completed"))) {
            const std::string path=string_field(local,"path");
            // Avatar file paths are supplied by our own TDLib instance.
            if (!path.empty() && avatar_attempted_.count(id)) avatar_files_[id]=path;
        }
    };
    if (type=="updateFile") cache_file(json_object_get(event,"file"));
    else if (type=="file") cache_file(event);
    else if (type=="updateChatPhoto") {
        auto chat=chats_.find(number(event,"chat_id"));
        if (chat!=chats_.end()) chat->second.photo_id=static_cast<int>(number(json_object_get(json_object_get(event,"photo"),"small"),"id"));
    }
    if (type == "updateNewChat") cache_chat(json_object_get(event, "chat"));
    else if (type == "chat") cache_chat(event);
    else if (type == "updateUser") cache_user(json_object_get(event, "user"));
    else if (type == "user") cache_user(event);
    else if (type == "updateChatTitle") { chats_dirty_ = true; auto it = chats_.find(number(event, "chat_id")); if (it != chats_.end()) it->second.title = string_field(event, "title"); }
    else if (type == "updateChatPosition" || type == "updateChatLastMessage") {
        chats_dirty_ = true;
        auto it = chats_.find(number(event, "chat_id"));
        if (it != chats_.end()) {
            if (type == "updateChatPosition") position(it->second, json_object_get(event, "position"));
            else { it->second.preview = message_text(json_object_get(event, "last_message")); it->second.order = 0; auto ps = json_object_get(event, "positions"); size_t i; json_t* p; json_array_foreach(ps, i, p) position(it->second, p); }
        }
    }
    if (history_chat_ && type == "updateNewMessage") {
        auto value = json_object_get(event, "message");
        if (number(value, "chat_id") == history_chat_) {
            const auto mid = number(value, "id");
            if (std::none_of(messages_.begin(), messages_.end(), [mid](const Message& m) { return m.id == mid; })) {
                messages_.insert(messages_.begin(), parse_message(value));
                if (messages_.size() > 500) messages_.resize(500);
            }
        }
    } else if (number(event, "chat_id") == history_chat_ && type == "updateMessageContent") {
        for (auto& message : messages_) if (message.id == number(event, "message_id")) {
            auto wrapper = json_object(); json_object_set(wrapper, "content", json_object_get(event, "new_content"));
            message.text = message_text(wrapper); parse_media(message,json_object_get(wrapper,"content")); json_decref(wrapper);
        }
    } else if (number(event, "chat_id") == history_chat_ && type == "updateDeleteMessages") {
        size_t i; json_t* value; json_array_foreach(json_object_get(event, "message_ids"), i, value) {
            const auto mid = json_integer_value(value);
            messages_.erase(std::remove_if(messages_.begin(), messages_.end(), [mid](const Message& m) { return m.id == mid; }), messages_.end());
        }
    }
    const int extra = static_cast<int>(number(event, "@extra"));
    auto pending = data_requests_.find(extra); if (pending == data_requests_.end()) return;
    const auto kind = pending->second.first; const auto id = pending->second.second; data_requests_.erase(pending);
    if ((kind == "saved" || kind == "private") && id != history_generation_) return;
    if (type == "error") {
        if (kind=="media") { auto file=media_files_.find(static_cast<int>(id)); if (file!=media_files_.end()) { file->second.pending=false; file->second.failed=true; } }
        if (kind == "load" && number(event, "code") == 404) { chats_status_ = "All main-list chats loaded."; fetch_chat_list(); }
        else if (kind == "load" || kind == "chats" || kind=="list_chat") chats_status_ = "Unable to load chats (error "+std::to_string(number(event,"code"))+"). Select Chats to retry.";
        else if (kind == "contacts" || kind == "user") contacts_status_ = "Unable to load some contacts. Square: retry.";
        else if ((kind == "history" && id == history_chat_) || kind == "saved" || kind == "private") history_status_ = "Unable to load chat. Circle: back; try again.";
        return;
    }
    if (kind == "load" && type == "ok") { chats_status_.clear(); fetch_chat_list(); }
    else if (kind=="chats" && type=="chats") {
        size_t i; json_t* value;
        json_array_foreach(json_object_get(event,"chat_ids"),i,value) {
            const auto cid=json_integer_value(value);
            data_request(json_pack("{s:s,s:I}","@type","getChat","chat_id",static_cast<json_int_t>(cid)),"list_chat",cid);
        }
        chats_status_=json_array_size(json_object_get(event,"chat_ids"))==0 ? "No Telegram chats." : "";
    }
    else if (kind == "contacts" && type == "users") {
        contacts_dirty_ = true;
        contact_ids_.clear(); size_t i; json_t* value;
        json_array_foreach(json_object_get(event, "user_ids"), i, value) {
            if (contact_ids_.size() >= 1000) break;
            const auto uid = json_integer_value(value); contact_ids_.push_back(uid);
            data_request(json_pack("{s:s,s:I}", "@type", "getUser", "user_id", static_cast<json_int_t>(uid)), "user", uid);
        }
        contacts_status_ = contact_ids_.empty() ? "No Telegram contacts." : "";
    } else if ((kind == "saved" || kind == "private") && type == "chat") {
        open_chat(number(event, "id")); if (kind == "saved") history_title_ = "Saved Messages";
    } else if (kind == "history" && id == history_chat_ && type == "messages") {
        const auto before = messages_.size(); size_t i; json_t* value;
        json_array_foreach(json_object_get(event, "messages"), i, value) {
            const auto mid = number(value, "id");
            if (std::none_of(messages_.begin(), messages_.end(), [mid](const Message& m) { return m.id == mid; }))
                messages_.push_back(parse_message(value));
        }
        std::sort(messages_.begin(), messages_.end(), [](const Message& a, const Message& b) { return a.id > b.id; });
        history_end_ = messages_.size() == before || messages_.size() >= 500;
        if (messages_.size() > 500) messages_.resize(500);
        history_status_ = messages_.empty() ? "No messages yet." : history_end_ ? "End of loaded history" : "Square: load older messages";
        // TDLib can return fewer than the requested limit while warming its cache.
        // Continue only while progressing, until the initial minimum or the end.
        if (initial_history_ && !history_end_ && messages_.size()<config::initial_history_messages) more_history();
        else initial_history_ = false;
    }
}

Auth::~Auth() {
    if (client_) td_json_client_destroy(client_);
    wipe(credentials_.api_hash);
}

bool Auth::start(const Credentials& credentials) {
    if (client_) return false;
    if (credentials.api_id <= 0 || credentials.api_hash.size() != 32) return false;
    // Use the system RNG, not clock/process IDs, to seed the OpenSSL engine.
    std::array<unsigned char, 64> entropy{};
    if (sceKernelGetRandomNumber(entropy.data(), entropy.size()) < 0) {
        stage_ = AuthStage::Failed; status_ = "System random source unavailable"; detail_.clear(); return false;
    }
    RAND_seed(entropy.data(), entropy.size());
    volatile unsigned char* p = entropy.data();
    for (std::size_t i = 0; i < entropy.size(); ++i) p[i] = 0;
    if (RAND_status() != 1) { stage_ = AuthStage::Failed; status_ = "Crypto initialization failed"; return false; }
    td_json_client_execute(nullptr, "{\"@type\":\"setLogStream\",\"log_stream\":{\"@type\":\"logStreamEmpty\"}}");
#ifdef VITA_TG_AUTH_TEST_DIAGNOSTICS
    // Isolated fake-credential harness only; never enabled in the application.
    extern void auth_test_log(int, const char*);
    td_set_log_message_callback(0, auth_test_log);
    td_json_client_execute(nullptr, "{\"@type\":\"setLogVerbosityLevel\",\"new_verbosity_level\":0}");
#endif
    credentials_ = credentials;
    client_ = td_json_client_create();
    if (!client_) { stage_ = AuthStage::Failed; status_ = "Client initialization failed"; return false; }
    stage_ = AuthStage::Starting;
    status_ = "Starting Telegram";
    detail_ = credentials.test_dc ? "Test server account" : "Connecting to Telegram";
    return true;
}

void Auth::initialize_parameters() {
    // The first port intentionally uses the binlog only; SQLite caching follows
    // after Vita file-locking/VFS semantics are validated.
    // Persist a random binlog key separately. Protect both files when copying
    // device backups; a nearby key does not protect against full-device access.
    std::array<unsigned char, 32> key{};
    const std::string key_path = data_directory_ + (credentials_.test_dc ? "/session-test.key" : "/session.key");
    int fd = ::open(key_path.c_str(), O_RDONLY);
    bool key_ok = false;
    if (fd >= 0) {
        key_ok = ::read(fd, key.data(), key.size()) == static_cast<ssize_t>(key.size());
        unsigned char extra = 0;
        key_ok = key_ok && ::read(fd, &extra, 1) == 0;
        ::close(fd);
    } else if (errno == ENOENT && sceKernelGetRandomNumber(key.data(), key.size()) == 0) {
        fd = ::open(key_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            key_ok = ::write(fd, key.data(), key.size()) == static_cast<ssize_t>(key.size());
            key_ok = key_ok && ::fsync(fd) == 0;
            ::close(fd);
            if (!key_ok) ::unlink(key_path.c_str());
        }
    }
    if (!key_ok) {
        stage_ = AuthStage::Failed;
        status_ = "Unable to load or create the session key";
        detail_ = "Check the app data directory; keep any existing key";
        return;
    }
    std::array<unsigned char, 45> encoded_key{};
    EVP_EncodeBlock(encoded_key.data(), key.data(), key.size());
    json_t* request = json_object();
    json_object_set_new(request, "database_encryption_key", json_string(reinterpret_cast<char*>(encoded_key.data())));
    volatile unsigned char* key_bytes = key.data();
    for (std::size_t i = 0; i < key.size(); ++i) key_bytes[i] = 0;
    volatile unsigned char* encoded_bytes = encoded_key.data();
    for (std::size_t i = 0; i < encoded_key.size(); ++i) encoded_bytes[i] = 0;
    json_object_set_new(request, "@type", json_string("setTdlibParameters"));
    json_object_set_new(request, "@extra", json_integer(++serial_));
    pending_ = serial_;
    json_object_set_new(request, "api_id", json_integer(credentials_.api_id));
    json_object_set_new(request, "api_hash", json_string(credentials_.api_hash.c_str()));
    json_object_set_new(request, "use_test_dc", json_boolean(credentials_.test_dc));
    json_object_set_new(request, "database_directory", json_string((data_directory_ + (credentials_.test_dc ? "/session-test" : "/session")).c_str()));
    json_object_set_new(request, "files_directory", json_string((data_directory_ + (credentials_.test_dc ? "/files-test" : "/files")).c_str()));
    json_object_set_new(request, "use_file_database", json_false());
    json_object_set_new(request, "use_chat_info_database", json_false());
    json_object_set_new(request, "use_message_database", json_false());
    json_object_set_new(request, "use_secret_chats", json_false());
    json_object_set_new(request, "system_language_code", json_string("en"));
    json_object_set_new(request, "device_model", json_string("PlayStation Vita"));
    json_object_set_new(request, "system_version", json_string("Vita homebrew"));
    json_object_set_new(request, "application_version", json_string(VITA_TG_VERSION));
    dispatch(client_, request);
}

void Auth::send(const char* type, const char* field, const std::string& value) {
    if (!client_ || busy()) return;
    json_t* request = json_object();
    json_object_set_new(request, "@type", json_string(type));
    json_object_set_new(request, "@extra", json_integer(++serial_));
    pending_ = serial_;
    if (field) json_object_set_new(request, field, json_string(value.c_str()));
    if (std::strcmp(type, "setAuthenticationPhoneNumber") == 0) {
        json_t* settings = json_object();
        json_object_set_new(settings, "@type", json_string("phoneNumberAuthenticationSettings"));
        json_object_set_new(settings, "allow_flash_call", json_false());
        json_object_set_new(settings, "allow_missed_call", json_false());
        json_object_set_new(settings, "is_current_phone_number", json_false());
        json_object_set_new(settings, "has_unknown_phone_number", json_false());
        json_object_set_new(settings, "allow_sms_retriever_api", json_false());
        json_object_set_new(settings, "firebase_authentication_settings", json_null());
        json_object_set_new(settings, "authentication_tokens", json_array());
        json_object_set_new(request, "settings", settings);
    }
    dispatch(client_, request);
    detail_ = "Waiting for Telegram";
}

bool Auth::needs_input() const {
    return !busy() && has_input_stage();
}
bool Auth::has_input_stage() const {
    return (stage_ == AuthStage::Phone || stage_ == AuthStage::Email ||
        stage_ == AuthStage::EmailCode || stage_ == AuthStage::Code || stage_ == AuthStage::Password);
}
void Auth::submit(const std::string& input) {
    if (!needs_input() || input.empty()) return;
    switch (stage_) {
        case AuthStage::Phone: send("setAuthenticationPhoneNumber", "phone_number", input); break;
        case AuthStage::Email: send("setAuthenticationEmailAddress", "email_address", input); break;
        case AuthStage::EmailCode: {
            json_t* request = json_pack("{s:s,s:i,s:{s:s,s:s}}", "@type", "checkAuthenticationEmailCode",
                "@extra", ++serial_, "code", "@type", "emailAddressAuthenticationCode", "code", input.c_str());
            pending_ = serial_; dispatch(client_, request); break;
        }
        case AuthStage::Code: send("checkAuthenticationCode", "code", input); break;
        case AuthStage::Password: send("checkAuthenticationPassword", "password", input); break;
        default: break;
    }
}
int Auth::resend_seconds() const {
    auto remaining = resend_at_ - std::chrono::steady_clock::now();
    if (remaining <= std::chrono::steady_clock::duration::zero()) return 0;
    return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(remaining).count()) + 1;
}
bool Auth::can_resend() const { return stage_ == AuthStage::Code && can_resend_ && resend_seconds() == 0 && !busy(); }
void Auth::resend_code() {
    if (can_resend())
        send("resendAuthenticationCode");
}
void Auth::logout() { if (stage_ == AuthStage::Ready) send("logOut"); }
void Auth::close() { if (client_) { pending_ = 0; retry_at_ = {}; send("close"); stage_ = AuthStage::Closing; } }
void Auth::poll() {
    if (!client_) return;
    for (int i = 0; i < 32; ++i) {
        const char* response = td_json_client_receive(client_, 0.0);
        if (!response) break;
        handle(response);
    }
}
void Auth::handle(const char* response) {
    json_error_t ignored{};
    json_t* event = json_loads(response, 0, &ignored);
    if (!event) return;
    const std::string type = string_field(event, "@type");
    const int extra = static_cast<int>(json_integer_value(json_object_get(event, "@extra")));
    handle_data(event);
    if (extra && extra == profile_request_ && stage_ == AuthStage::Ready) {
        profile_request_ = 0;
        if (type == "user") {
            self_id_ = json_integer_value(json_object_get(event, "id"));
            account_name_ = string_field(event, "first_name");
            const std::string last_name = string_field(event, "last_name");
            if (!last_name.empty()) account_name_ += " " + last_name;
            if (saved_pending_) open_saved_messages();
        } else if (saved_pending_) {
            saved_pending_ = false; history_status_ = "Account unavailable. Circle: back; try again.";
        }
    }
    if (extra && extra == pending_) {
        pending_ = 0;
        if (type == "error") {
            // Don't display/log the raw response: it can contain private inputs.
            status_ = "Telegram rejected the request";
            detail_ = "Check your input and try again";
            const std::string message = string_field(event, "message");
            if (json_integer_value(json_object_get(event, "code")) == 429 || message.find("FLOOD_WAIT") != std::string::npos) {
                auto last = message.find_last_not_of("0123456789");
                const char* digits = message.c_str() + (last == std::string::npos ? 0 : last + 1);
                errno = 0;
                const long wait = std::strtol(digits, nullptr, 10);
                const long seconds = errno != ERANGE && wait > 0 && wait <= 2147483647L ? wait : 60;
                retry_at_ = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
                detail_ = "Please wait " + std::to_string(seconds) + " seconds before retrying";
            } else if (message == "PHONE_CODE_INVALID" || message == "CODE_INVALID") detail_ = "Incorrect code; try again";
            else if (message == "PHONE_CODE_EXPIRED") detail_ = "Code expired; request a new code when allowed";
            else if (message == "PASSWORD_HASH_INVALID") detail_ = "Incorrect password; try again";
            else if (message == "PHONE_NUMBER_INVALID") detail_ = "Check your phone number and country code";
            else if (message == "API_ID_INVALID") detail_ = "Check the app credentials in telegram.conf";
            else if (message == "EMAIL_INVALID") detail_ = "Check your login email address";
            if (stage_ == AuthStage::Starting) stage_ = AuthStage::Failed;
        }
    }
    if (type == "updateAuthorizationState") {
        json_t* state = json_object_get(event, "authorization_state");
        const std::string name = string_field(state, "@type");
        pending_ = 0;
        if (name != "authorizationStateWaitPassword") wipe(password_hint_);
        if (name != "authorizationStateReady") { account_name_.clear(); profile_request_ = 0; clear_data(); }
        detail_ = "Review your entry, then press Square to continue";
        if (name == "authorizationStateWaitTdlibParameters") initialize_parameters();
        else if (name == "authorizationStateWaitPhoneNumber") { stage_ = AuthStage::Phone; status_ = "Enter your phone number"; detail_ = "Include country code, for example +46"; }
        else if (name == "authorizationStateWaitEmailAddress") { stage_ = AuthStage::Email; status_ = "Enter your login email"; }
        else if (name == "authorizationStateWaitEmailCode") { stage_ = AuthStage::EmailCode; status_ = "Enter the email verification code"; }
        else if (name == "authorizationStateWaitCode") {
            stage_ = AuthStage::Code; status_ = "Enter your login code";
            json_t* info = json_object_get(state, "code_info");
            const std::string delivery = string_field(json_object_get(info, "type"), "@type");
            if (delivery == "authenticationCodeTypeTelegramMessage") detail_ = "Check Telegram on your other signed-in device";
            else if (delivery == "authenticationCodeTypeSms" || delivery == "authenticationCodeTypeSmsWord" || delivery == "authenticationCodeTypeSmsPhrase") detail_ = "Check your SMS messages";
            else if (delivery == "authenticationCodeTypeCall" || delivery == "authenticationCodeTypeMissedCall") detail_ = "Use the code from Telegram's phone call";
            else if (delivery == "authenticationCodeTypeFragment") detail_ = "Check the code on Fragment using your other device";
            else { stage_ = AuthStage::Unsupported; detail_ = "This code delivery method requires another client"; }
            json_t* next = json_object_get(info, "next_type");
            can_resend_ = next && !json_is_null(next);
            const auto timeout = json_integer_value(json_object_get(info, "timeout"));
            resend_at_ = std::chrono::steady_clock::now() + std::chrono::seconds(std::min<json_int_t>(2147483647, std::max<json_int_t>(0, timeout)));
        }
        else if (name == "authorizationStateWaitPassword") { stage_ = AuthStage::Password; status_ = "Two-step verification";
            password_hint_ = string_field(state, "password_hint");
            detail_ = "Enter the password you set in Telegram";
        }
        else if (name == "authorizationStateReady") { stage_ = AuthStage::Ready; status_ = "Login successful"; detail_ = "Your Telegram session is active";
            if (client_) {
                json_t* request = json_pack("{s:s,s:i}", "@type", "getMe", "@extra", ++serial_);
                profile_request_ = serial_; dispatch(client_, request);
                refresh_chats(); refresh_contacts();
            }
        }
        else if (name == "authorizationStateWaitRegistration") { stage_ = AuthStage::Registration; status_ = "Account registration required"; detail_ = "Create an account in the official app first"; }
        else if (name == "authorizationStateClosing" || name == "authorizationStateLoggingOut") { stage_ = AuthStage::Closing; status_ = "Closing Telegram"; detail_.clear(); }
        else if (name == "authorizationStateClosed") { stage_ = AuthStage::Closed; status_ = "Telegram closed"; detail_.clear(); }
        else { stage_ = AuthStage::Unsupported; status_ = "Additional authorization step required"; detail_ = "This login flow is not yet supported"; }
    }
    json_decref(event);
}
}
