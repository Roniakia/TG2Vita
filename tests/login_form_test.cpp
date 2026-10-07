#include "ui/login_form.hpp"
#include <cassert>
#include <iostream>
int main() {
    using telegram::AuthStage;
    LoginForm form; std::string error;
    form.sync(AuthStage::Phone);
    assert(!form.validate(error));
    form.set("+46 70 123 45 67"); assert(form.validate(error));
    form.set("0701234567"); assert(!form.validate(error));
    form.set("+46 abc"); assert(!form.validate(error));
    form.sync(AuthStage::Code); assert(form.value().empty());
    form.set("phrase code"); assert(form.validate(error));
    form.sync(AuthStage::Password); assert(form.value().empty());
    form.set("private password"); assert(form.validate(error));
    assert(form.password()); assert(form.display()=="********");
    assert(form.display().find("private")==std::string::npos);
    form.sync(AuthStage::Ready); assert(form.value().empty()); assert(form.step()==3);
    form.sync(AuthStage::Password); form.set("secret");form.clear();assert(form.value().empty());
    std::cout << "Login form validation and password masking checks passed\n";
}
