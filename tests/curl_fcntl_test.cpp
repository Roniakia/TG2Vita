#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cassert>
extern "C" int vita_tg_curl_fcntl(int,int,int);
int main() {
 int fd=socket(AF_INET,SOCK_STREAM,0); assert(fd>=0);
 assert(vita_tg_curl_fcntl(fd,F_SETFD,FD_CLOEXEC)==0);
 assert(vita_tg_curl_fcntl(-1,F_SETFD,FD_CLOEXEC)==-1 && errno==EBADF);
 assert(vita_tg_curl_fcntl(fd,F_SETFL,0)==-1 && errno==EINVAL);
 close(fd);
 assert(vita_tg_curl_fcntl(fd,F_SETFD,FD_CLOEXEC)==-1 && errno==EBADF);
}
