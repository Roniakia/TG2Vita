#pragma once
#include "telegram/auth.hpp"
namespace telegram {
struct AuthTestAccess {
    static bool run() {
        Auth auth("ux0:data/vita-tg-auth-tests");
        // Synthetic state tests never create an engine or send a login request.
        auth.handle(R"({"@type":"updateAuthorizationState","authorization_state":{"@type":"authorizationStateWaitPhoneNumber"}})");
        if (auth.stage()!=AuthStage::Phone || !auth.needs_input()) return false;
        auth.handle(R"({"@type":"updateAuthorizationState","authorization_state":{"@type":"authorizationStateWaitCode","code_info":{"type":{"@type":"authenticationCodeTypeTelegramMessage"},"next_type":{"@type":"authenticationCodeTypeSms"},"timeout":0}}})");
        if (auth.stage()!=AuthStage::Code || !auth.can_resend()) return false;
        auth.pending_=7;
        auth.handle(R"({"@type":"error","@extra":7,"code":400,"message":"PHONE_CODE_INVALID"})");
        if (auth.stage()!=AuthStage::Code || auth.busy() || auth.detail()!="Incorrect code; try again") return false;
        auth.handle(R"({"@type":"updateAuthorizationState","authorization_state":{"@type":"authorizationStateWaitPassword","password_hint":"test hint"}})");
        if (auth.stage()!=AuthStage::Password || auth.password_hint()!="test hint") return false;
        auth.pending_=8;
        auth.handle(R"({"@type":"error","@extra":8,"code":400,"message":"PASSWORD_HASH_INVALID"})");
        if (auth.stage()!=AuthStage::Password || auth.detail()!="Incorrect password; try again") return false;
        auth.handle(R"({"@type":"updateAuthorizationState","authorization_state":{"@type":"authorizationStateReady"}})");
        if (auth.stage()!=AuthStage::Ready || !auth.password_hint().empty()) return false;
        auth.profile_request_=19;
        auth.handle(R"({"@type":"user","@extra":19,"first_name":"Test","last_name":"Account"})");
        if (auth.account_name()!="Test Account") return false;
        auth.handle(R"({"@type":"updateAuthorizationState","authorization_state":{"@type":"authorizationStateClosed"}})");
        if (auth.stage()!=AuthStage::Closed || !auth.account_name().empty()) return false;
        return true;
    }
};
}
