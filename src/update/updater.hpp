#pragma once
#include "release.hpp"
#include <atomic>
#include <mutex>
#include <thread>
namespace update {
struct State { std::string status="Cross: check for updates", detail; bool busy=false, available=false; unsigned percent=0; };
class Updater {
public:
    Updater();
    ~Updater();
    State state() const;
    void check();
    void download();
    void cancel();
private:
    void run(bool download);
    void finish(const std::string& status,const std::string& detail={},bool available=false);
    mutable std::mutex mutex_;
    State state_;
    Release release_;
    std::atomic<bool> cancel_{false};
    std::thread worker_;
    bool initialized_=false;
};
}
