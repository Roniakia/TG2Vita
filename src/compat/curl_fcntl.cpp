#include <fcntl.h>
#include <sys/socket.h>
#include <cerrno>

// Only the private curl archive calls this symbol. Its installed version uses
// fcntl solely as fcntl(fd, F_SETFD, FD_CLOEXEC). Vita has no exec inheritance.
extern "C" int vita_tg_curl_fcntl(int fd,int command,int flags) {
    if (command!=F_SETFD || flags!=FD_CLOEXEC) {
        errno=EINVAL;
        return -1;
    }
    // Preserve invalid-descriptor errors rather than blindly claiming success.
    int type=0;
    socklen_t size=sizeof(type);
    return getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&size);
}
