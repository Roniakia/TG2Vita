#pragma once
#include "telegram/auth.hpp"
#include <string>

// Input is held only while reviewing the current authorization step.
class LoginForm {
public:
    ~LoginForm() { clear(); }
    void sync(telegram::AuthStage stage);
    void set(std::string value);
    void clear();
    const std::string& value() const { return value_; }
    std::string display() const;
    bool validate(std::string& error) const;
    const char* label() const;
    const char* placeholder() const;
    const char* action() const;
    const char16_t* title() const;
    bool password() const { return stage_ == telegram::AuthStage::Password; }
    int step() const;
private:
    telegram::AuthStage stage_ = telegram::AuthStage::MissingConfig;
    std::string value_;
};
