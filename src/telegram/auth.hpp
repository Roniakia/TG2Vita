#pragma once
#include <string>
#include <chrono>
#include <utility>
#include <vector>
#include <map>
#include <cstdint>

struct json_t;

namespace telegram {
enum class AuthStage { MissingConfig, Starting, Phone, Email, EmailCode, Code, Password,
                       Ready, Registration, Unsupported, Closing, Closed, Failed };
struct Credentials {
    int api_id = 0;
    std::string api_hash;
    bool test_dc = true;
};
bool load_application_credentials(Credentials& result, std::string& error);
bool load_credentials(const char* path, Credentials& result, std::string& error);
struct Chat { std::int64_t id = 0, order = 0; std::string title, preview; int photo_id = 0; };
struct Contact { std::int64_t id = 0; std::string name; int photo_id = 0; };
enum class MediaKind { None, Photo, Video, Animation, Unsupported };
struct Media {
    MediaKind kind = MediaKind::None;
    int preview_id = 0, file_id = 0, width = 0, height = 0;
    std::int64_t preview_size = 0, file_size = 0;
    bool spoiler = false, secret = false;
    std::string label;
};
struct MediaFile {
    std::string path;
    std::int64_t downloaded = 0, size = 0, limit = 0;
    bool pending = false, failed = false;
};
struct Message { std::int64_t id = 0; std::string text; bool outgoing = false; std::int64_t date = 0;
    std::int64_t sender_id = 0, forward_id = 0;
    bool sender_chat = false, forward_chat = false, forwarded = false;
    std::string forward_hidden_name, forward_signature, author_signature;
    Media media;
};
class Auth {
public:
    explicit Auth(std::string data_directory = "ux0:data/vita-tg")
        : data_directory_(std::move(data_directory)) {}
    ~Auth();
    Auth(const Auth&) = delete;
    Auth& operator=(const Auth&) = delete;
    bool start(const Credentials& credentials);
    void poll();
    void submit(const std::string& input);
    void resend_code();
    void logout();
    void close();
    AuthStage stage() const { return stage_; }
    bool busy() const { return pending_ != 0 || std::chrono::steady_clock::now() < retry_at_; }
    const std::string& status() const { return status_; }
    const std::string& detail() const { return detail_; }
    bool needs_input() const;
    bool has_input_stage() const;
    bool test_dc() const { return credentials_.test_dc; }
    const std::string& account_name() const { return account_name_; }
    const std::string& password_hint() const { return password_hint_; }
    int resend_seconds() const;
    bool can_resend() const;
    std::string sender_name(const Message& message) const;
    std::string forwarding_title(const Message& message) const;
    std::string avatar_path(const Message& message) const;
    void request_avatar(const Message& message);
    void request_media(const Message& message, bool full = false, bool retry = false);
    MediaFile media_file(int id) const;
    void cancel_media(int id);
    void refresh_chats();
    void refresh_contacts();
    void open_saved_messages();
    void open_chat(std::int64_t id);
    void open_contact(std::int64_t id);
    void more_history();
    void prefetch_history();
    void leave_chat();
    const std::vector<Chat>& chats() const;
    const std::vector<Contact>& contacts() const;
    const std::vector<Message>& messages() const { return messages_; }
    const std::string& history_title() const { return history_title_; }
    const std::string& chats_status() const { return chats_status_; }
    const std::string& contacts_status() const { return contacts_status_; }
    const std::string& history_status() const { return history_status_; }
    friend struct AuthTestAccess;
    friend struct AuthDataTestAccess;
private:
    void send(const char* type, const char* field = nullptr, const std::string& value = {});
    void handle(const char* response);
    void initialize_parameters();
    void data_request(json_t* request, const std::string& kind, std::int64_t id = 0);
    void handle_data(json_t* event);
    void clear_data();
    Message parse_message(json_t* value);
    void parse_media(Message& message, json_t* content);
    void cache_media_file(json_t* file);
    std::map<int, MediaFile> media_files_;
    void resolve_sender(std::int64_t id, bool chat);
    std::string identity_name(std::int64_t id, bool chat) const;
    void fetch_chat_list();
    std::map<int, std::string> avatar_files_;
    std::map<int, bool> avatar_attempted_;
    std::map<int, std::pair<std::string, std::int64_t>> data_requests_;
    std::map<std::int64_t, Chat> chats_;
    std::map<std::int64_t, Contact> users_;
    std::vector<std::int64_t> contact_ids_;
    std::vector<Message> messages_;
    mutable std::vector<Chat> chat_rows_;
    mutable std::vector<Contact> contact_rows_;
    mutable bool chats_dirty_ = true, contacts_dirty_ = true;
    std::int64_t self_id_ = 0, history_chat_ = 0;
    std::int64_t history_generation_ = 0, prefetch_cursor_ = 0;
    bool saved_pending_ = false, history_end_ = false, initial_history_ = false;
    std::string history_title_, chats_status_, contacts_status_, history_status_;
    std::string data_directory_;
    void* client_ = nullptr;
    Credentials credentials_;
    AuthStage stage_ = AuthStage::MissingConfig;
    std::string status_ = "Credentials required";
    std::string detail_ = "Copy telegram.conf to ux0:data/vita-tg/";
    std::chrono::steady_clock::time_point retry_at_{};
    std::chrono::steady_clock::time_point resend_at_{};
    bool can_resend_ = false;
    int serial_ = 0;
    int pending_ = 0;
    int profile_request_ = 0;
    std::string account_name_;
    std::string password_hint_;
};
}
