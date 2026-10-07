#include "login_form.hpp"
#include <utility>

void LoginForm::clear() {
    volatile char* p = value_.empty() ? nullptr : &value_[0];
    for (std::size_t i=0; i<value_.size(); ++i) p[i]=0;
    value_.clear();
}
void LoginForm::sync(telegram::AuthStage stage) {
    if (stage_ != stage) { clear(); stage_=stage; }
}
void LoginForm::set(std::string value) { clear(); value_=std::move(value); }
std::string LoginForm::display() const {
    if (value_.empty()) return placeholder();
    return password() ? "********" : value_;
}
bool LoginForm::validate(std::string& error) const {
    error.clear();
    if (value_.empty()) { error="Enter " + std::string(label()) + " first"; return false; }
    if (stage_ == telegram::AuthStage::Phone) {
        std::size_t digits=0;
        for (std::size_t i=0; i<value_.size(); ++i) {
            char c=value_[i];
            if (c>='0' && c<='9') ++digits;
            else if (!(c=='+' && i==0) && c!=' ' && c!='-' && c!='(' && c!=')') {
                error="Use a phone number with its country code"; return false;
            }
        }
        if (value_[0]!='+' || digits<5 || digits>15) { error="Include + and your country code"; return false; }
    }
    return true;
}
const char* LoginForm::label() const {
    using telegram::AuthStage;
    switch(stage_) {
        case AuthStage::Phone: return "Phone number";
        case AuthStage::Email: return "Login email";
        case AuthStage::EmailCode: return "Email verification code";
        case AuthStage::Code: return "Confirmation code";
        case AuthStage::Password: return "2FA password";
        default: return "Details";
    }
}
const char* LoginForm::placeholder() const {
    using telegram::AuthStage;
    switch(stage_) {
        case AuthStage::Phone: return "+ country code and number";
        case AuthStage::Email: return "Your Telegram login email";
        case AuthStage::EmailCode: case AuthStage::Code: return "Enter the code you received";
        case AuthStage::Password: return "Enter your two-step password";
        default: return "";
    }
}
const char* LoginForm::action() const {
    return stage_ == telegram::AuthStage::Phone ? "Send login code" :
           stage_ == telegram::AuthStage::Password ? "Sign in" : "Continue";
}
const char16_t* LoginForm::title() const {
    using telegram::AuthStage;
    switch(stage_) {
        case AuthStage::Phone: return u"Phone number with country code";
        case AuthStage::Email: return u"Telegram login email";
        case AuthStage::EmailCode: return u"Email verification code";
        case AuthStage::Password: return u"Two-step verification password";
        default: return u"Telegram confirmation code";
    }
}
int LoginForm::step() const {
    using telegram::AuthStage;
    switch(stage_) {
        case AuthStage::Phone: return 0;
        case AuthStage::Email: case AuthStage::EmailCode: case AuthStage::Code: return 1;
        case AuthStage::Password: return 2;
        case AuthStage::Ready: return 3;
        default: return -1;
    }
}
