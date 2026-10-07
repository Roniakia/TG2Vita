#include "telegram/auth.hpp"
#include "vita-auth/state_checks.hpp"
#include <td/telegram/td_json_client.h>
#include <jansson.h>
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
static std::vector<std::string> requests;
extern "C" {
void* td_json_client_create() { return reinterpret_cast<void*>(1); }
void td_json_client_destroy(void*) {}
void td_json_client_send(void*, const char* value) { requests.emplace_back(value); }
const char* td_json_client_receive(void*, double) { return nullptr; }
const char* td_json_client_execute(void*, const char*) { return nullptr; }
}
namespace telegram {
struct AuthDataTestAccess {
    static void run() {
        Auth auth;
        auth.client_ = reinterpret_cast<void*>(1);
        auth.stage_ = AuthStage::Ready;
        auth.handle(R"({"@type":"updateNewChat","chat":{"id":101,"title":"First","last_message":{"content":{"@type":"messageText","text":{"text":"Chat preview"}}},"positions":[{"list":{"@type":"chatListMain"},"order":"900000000000000001"}]}})");
        auth.handle(R"({"@type":"updateNewChat","chat":{"id":102,"title":"Second","positions":[{"list":{"@type":"chatListMain"},"order":"900000000000000002"}]}})");
        assert(auth.chats().size()==2 && auth.chats()[0].id==102);
        auth.handle(R"({"@type":"updateChatPosition","chat_id":102,"position":{"list":{"@type":"chatListMain"},"order":"0"}})");
        assert(auth.chats().size()==1 && auth.chats()[0].id==101 && auth.chats()[0].preview=="Chat preview");
        auth.refresh_chats(); const auto serial=auth.serial_;
        auth.refresh_chats(); assert(auth.serial_==serial); // deduplicates in-flight loads
        auth.handle(("{\"@type\":\"error\",\"@extra\":"+std::to_string(serial)+",\"code\":404}").c_str());
        assert(auth.chats_status()=="All main-list chats loaded.");
        const auto list_request=auth.serial_;
        auth.handle(("{\"@type\":\"chats\",\"@extra\":"+std::to_string(list_request)+",\"chat_ids\":[101,102]}").c_str());
        assert(auth.data_requests_.count(auth.serial_) && auth.data_requests_[auth.serial_].first=="list_chat");
        auth.refresh_contacts();
        auth.handle(("{\"@type\":\"users\",\"@extra\":"+std::to_string(auth.serial_)+",\"user_ids\":[7,8]}").c_str());
        auth.handle(R"({"@type":"updateUser","user":{"id":7,"first_name":"Zoe"}})");
        auth.handle(R"({"@type":"updateUser","user":{"id":8,"first_name":"Alice"}})");
        assert(auth.contacts().size()==2 && auth.contacts()[0].name=="Alice");
        auth.self_id_=55;
        auth.open_saved_messages(); const auto saved=auth.serial_;
        json_t* request=json_loads(requests.back().c_str(),0,nullptr);
        assert(std::string(json_string_value(json_object_get(request,"@type")))=="createPrivateChat");
        assert(json_integer_value(json_object_get(request,"user_id"))==55); json_decref(request);
        auth.handle(("{\"@type\":\"chat\",\"@extra\":"+std::to_string(saved)+",\"id\":55,\"title\":\"Personal\",\"positions\":[]}").c_str());
        assert(auth.history_chat_==55 && auth.history_title()=="Saved Messages");
        const auto history=auth.serial_;
        auth.handle(("{\"@type\":\"messages\",\"@extra\":"+std::to_string(history)+R"(,"messages":[{"id":3,"is_outgoing":true,"date":1791324000,"content":{"@type":"messageText","text":{"text":"Saved note"}}},{"id":2,"content":{"@type":"messagePhoto"}}]})").c_str());
        assert(auth.messages().size()==2 && auth.messages()[0].text=="Saved note" && auth.messages()[0].date==1791324000);
        auth.more_history();
        request=json_loads(requests.back().c_str(),0,nullptr);
        assert(json_integer_value(json_object_get(request,"from_message_id"))==2); json_decref(request);
        auth.handle(("{\"@type\":\"messages\",\"@extra\":"+std::to_string(auth.serial_)+R"(,"messages":[{"id":2,"content":{"@type":"messagePhoto"}},{"id":1,"content":{"@type":"messageText","text":{"text":"Older"}}}]})").c_str());
        assert(auth.messages().size()==3);
        auth.handle(R"({"@type":"updateMessageContent","chat_id":55,"message_id":3,"new_content":{"@type":"messageText","text":{"text":"Edited"}}})");
        assert(auth.messages()[0].text=="Edited");
        auth.handle(R"({"@type":"updateDeleteMessages","chat_id":55,"message_ids":[2]})");
        assert(auth.messages().size()==2);
        auth.open_contact(7); const auto abandoned=auth.serial_;
        auth.open_chat(101);
        auth.handle(("{\"@type\":\"chat\",\"@extra\":"+std::to_string(abandoned)+",\"id\":7,\"title\":\"Zoe\"}").c_str());
        assert(auth.history_chat_==101); // late responses can't replace the current chat
        auth.open_saved_messages(); const auto canceled=auth.serial_; auth.leave_chat();
        auth.handle(("{\"@type\":\"chat\",\"@extra\":"+std::to_string(canceled)+",\"id\":55,\"title\":\"Personal\"}").c_str());
        assert(auth.history_chat_==0 && auth.messages().empty());
        auth.handle(R"({"@type":"updateUser","user":{"id":77,"first_name":"Ada","last_name":"Lovelace","profile_photo":{"small":{"id":321}}}})");
        json_t* sender_fixture=json_loads(R"({"id":700,"sender_id":{"@type":"messageSenderUser","user_id":77},"forward_info":{"origin":{"@type":"messageOriginHiddenUser","sender_name":"Private Author"}},"content":{"@type":"messageText","text":{"text":"Forwarded note"}}})",0,nullptr);
        auto sender=auth.parse_message(sender_fixture); json_decref(sender_fixture);
        assert(auth.sender_name(sender)=="Ada Lovelace");
        assert(auth.forwarding_title(sender)=="Forwarded from Private Author");
        auth.request_avatar(sender); const auto avatar_request=auth.serial_;
        auth.request_avatar(sender); assert(auth.serial_==avatar_request);
        auth.handle(R"({"@type":"updateFile","file":{"id":321,"local":{"is_downloading_completed":true,"path":"/mock/avatar.jpg"}}})");
        assert(auth.avatar_path(sender)=="/mock/avatar.jpg");
        auth.handle(R"({"@type":"updateNewChat","chat":{"id":-700,"title":"Science Channel","positions":[]}})");
        json_t* fixture=json_loads(R"({"id":701,"sender_id":{"@type":"messageSenderChat","chat_id":-700},"author_signature":"Editor","forward_info":{"origin":{"@type":"messageOriginChannel","chat_id":-700,"author_signature":"Original Author"}},"content":{"@type":"messageText","text":{"text":"Channel forward"}}})",0,nullptr);
        auto channel=auth.parse_message(fixture); json_decref(fixture);
        assert(auth.sender_name(channel)=="Science Channel · Editor");
        assert(auth.forwarding_title(channel)=="Forwarded from Science Channel · Original Author");
        fixture=json_loads(R"({"id":702,"sender_id":{"@type":"messageSenderUser","user_id":77},"forward_info":{"origin":{"@type":"messageOriginUser","sender_user_id":8}},"content":{"@type":"messageText","text":{"text":"User forward"}}})",0,nullptr);
        auto user_forward=auth.parse_message(fixture); json_decref(fixture);
        assert(auth.forwarding_title(user_forward)=="Forwarded from Alice");
        auth.open_chat(999);
        for (int i=0;i<10;++i) {
            const auto request_id=auth.serial_;
            auth.handle(("{\"@type\":\"messages\",\"@extra\":"+std::to_string(request_id)+",\"messages\":[{\"id\":"+std::to_string(100-i)+",\"content\":{\"@type\":\"messageText\",\"text\":{\"text\":\"Batch\"}}}]}").c_str());
            assert(auth.messages().size()==static_cast<std::size_t>(i+1));
            if (i<9) assert(auth.serial_>request_id);
            else assert(auth.serial_==request_id && !auth.initial_history_);
        }
        auth.prefetch_history(); const auto prefetch=auth.serial_;
        auth.prefetch_history(); assert(auth.serial_==prefetch); // Only one in-flight request.
        auth.handle(("{\"@type\":\"error\",\"@extra\":"+std::to_string(prefetch)+",\"code\":500}").c_str());
        auth.prefetch_history(); assert(auth.serial_==prefetch); // No automatic retry loop on errors.
        auth.more_history(); assert(auth.serial_>prefetch); // Manual retry remains available.
        auth.open_chat(888);
        const auto empty_request=auth.serial_;
        auth.handle(("{\"@type\":\"messages\",\"@extra\":"+std::to_string(empty_request)+",\"messages\":[]}").c_str());
        assert(auth.serial_==empty_request && auth.history_end_ && !auth.initial_history_);
        // Pinned TDLib media fixtures: bounded variants, caption preservation,
        // lazy downloads, updates, failure/retry and privacy-sensitive media.
        auto parse=[&](const char* json) { auto value=json_loads(json,0,nullptr); auto result=auth.parse_message(value); json_decref(value); return result; };
        auto photo=parse(R"({"id":500,"content":{"@type":"messagePhoto","caption":{"text":"Complete caption\nsecond line"},"photo":{"sizes":[{"width":90,"height":90,"photo":{"id":900,"size":3000}},{"width":320,"height":240,"photo":{"id":901,"size":12000}},{"width":1280,"height":960,"photo":{"id":902,"size":250000}},{"width":8000,"height":6000,"photo":{"id":903,"size":20000000}}]}}})");
        assert(photo.media.kind==MediaKind::Photo && photo.media.preview_id==901 && photo.media.file_id==902);
        assert(photo.text=="Complete caption\nsecond line");
        const auto before_media=auth.serial_; auth.request_media(photo); const auto preview_serial=auth.serial_;
        assert(preview_serial>before_media); auth.request_media(photo); assert(auth.serial_==preview_serial);
        auto download=json_loads(requests.back().c_str(),0,nullptr);
        assert(std::string(json_string_value(json_object_get(download,"@type")))=="downloadFile");
        assert(json_integer_value(json_object_get(download,"file_id"))==901); json_decref(download);
        auth.handle(R"({"@type":"updateFile","file":{"id":901,"size":12000,"local":{"is_downloading_active":true,"downloaded_size":6000}}})");
        assert(auth.media_file(901).pending && auth.media_file(901).downloaded==6000);
        auth.handle(("{\"@type\":\"file\",\"@extra\":"+std::to_string(preview_serial)+R"(,"id":901,"size":12000,"local":{"is_downloading_completed":true,"path":"/mock/preview.jpg"}})").c_str());
        assert(auth.media_file(901).path=="/mock/preview.jpg" && !auth.media_file(901).pending);
        auth.request_media(photo,true); const auto full_serial=auth.serial_;
        auth.handle(("{\"@type\":\"error\",\"@extra\":"+std::to_string(full_serial)+",\"code\":500}").c_str());
        assert(auth.media_file(902).failed); auth.request_media(photo,true); assert(auth.serial_==full_serial);
        auth.request_media(photo,true,true); assert(auth.serial_>full_serial && auth.media_file(902).pending);
        auto spoiler=photo; spoiler.media.spoiler=true; const auto spoiler_serial=auth.serial_;
        spoiler.media.preview_id=905; spoiler.media.file_id=906; auth.request_media(spoiler); assert(auth.serial_==spoiler_serial);
        auth.request_media(spoiler,true); assert(auth.serial_>spoiler_serial);
        auto secret=photo; secret.media.secret=true; secret.media.file_id=907;
        const auto secret_serial=auth.serial_; auth.request_media(secret,true); assert(auth.serial_==secret_serial);
        auto video=parse(R"({"id":501,"content":{"@type":"messageVideo","caption":{"text":"Movie"},"video":{"width":1280,"height":720,"video":{"id":910,"size":"1000000"},"thumbnail":{"format":{"@type":"thumbnailFormatJpeg"},"file":{"id":911,"size":8000}}}}})");
        assert(video.media.kind==MediaKind::Video && video.media.preview_id==911 && video.media.file_id==910 && video.text=="Movie");
        auth.request_media(video); const auto thumbnail_serial=auth.serial_; auth.request_media(video,true);
        assert(auth.serial_>thumbnail_serial); // Full movies are fetched only on selection.
        auto oversized=video; oversized.media.file_id=912; oversized.media.file_size=100000000;
        const auto limit_serial=auth.serial_; auth.request_media(oversized,true); assert(auth.serial_==limit_serial && auth.media_file(912).failed);
        auto unknown=video; unknown.media.file_id=913; unknown.media.file_size=0;
        auth.request_media(unknown,true);
        auth.handle(R"({"@type":"updateFile","file":{"id":913,"size":100000000,"local":{"is_downloading_active":true}}})");
        assert(auth.media_file(913).failed && !auth.media_file(913).pending);
        assert(requests.back().find("cancelDownloadFile")!=std::string::npos);
        const auto cancel_serial=auth.serial_;
        auth.handle(("{\"@type\":\"ok\",\"@extra\":"+std::to_string(cancel_serial)+"}").c_str());
        auth.handle(R"({"@type":"updateFile","file":{"id":913,"size":100000000,"local":{"is_downloading_active":false}}})");
        assert(auth.serial_==cancel_serial); // Canceled file updates cannot cause a request loop.
        auto video_note=parse(R"({"id":497,"content":{"@type":"messageVideoNote","video_note":{"length":384,"video":{"id":914,"size":10000}}}})");
        assert(video_note.media.kind==MediaKind::Video && video_note.media.file_id==914);
        auto service=parse(R"({"id":499,"content":{"@type":"messageChatJoinByLink"}})");
        assert(service.media.kind==MediaKind::None);
        auto document=parse(R"({"id":498,"content":{"@type":"messageDocument"}})");
        assert(document.media.kind==MediaKind::Unsupported);
        auto gif=parse(R"({"id":502,"content":{"@type":"messageAnimation","animation":{"mime_type":"image/gif","animation":{"id":920}}}})");
        assert(gif.media.kind==MediaKind::Unsupported);
        auto mp4=parse(R"({"id":503,"content":{"@type":"messageAnimation","animation":{"mime_type":"video/mp4","animation":{"id":921,"size":1000}}}})");
        assert(mp4.media.kind==MediaKind::Animation);
        auto local_photo=parse(R"({"id":504,"content":{"@type":"messagePhoto","photo":{"sizes":[{"width":320,"height":200,"photo":{"id":930,"size":1000,"local":{"is_downloading_completed":true,"path":"/mock/local.jpg"}}}]}}})");
        assert(auth.media_file(local_photo.media.file_id).path=="/mock/local.jpg");
        auth.history_chat_=888; auth.messages_={photo};
        auth.handle(R"({"@type":"updateMessageContent","chat_id":888,"message_id":500,"new_content":{"@type":"messageText","text":{"text":"Replacement"}}})");
        assert(auth.messages_[0].media.kind==MediaKind::None && auth.messages_[0].text=="Replacement");
        auth.handle(R"({"@type":"updateAuthorizationState","authorization_state":{"@type":"authorizationStateLoggingOut"}})");
        assert(auth.stage()==AuthStage::Closing && auth.chats().empty() && auth.contacts().empty() && auth.data_requests_.empty() && auth.avatar_files_.empty() && auth.avatar_attempted_.empty() && auth.media_files_.empty());
    }
};
}
int main() {
    assert(telegram::AuthTestAccess::run());
    telegram::AuthDataTestAccess::run();
    std::cout << "Authentication and browser state tests passed\n";
}
