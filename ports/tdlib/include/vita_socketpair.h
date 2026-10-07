#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
// Vita has no Unix-domain socketpair. Use connected loopback TCP for wakeups.
static int vita_socketpair(int, int type, int protocol, int fds[2]) {
  int listener = socket(AF_INET, type, protocol);
  if (listener < 0) return -1;
  int client = -1, accepted = -1;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  socklen_t size = sizeof(address);
  if (bind(listener, reinterpret_cast<sockaddr*>(&address), size) == 0 &&
      listen(listener, 1) == 0 &&
      getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0) {
    client = socket(AF_INET, type, protocol);
    if (client >= 0 && connect(client, reinterpret_cast<sockaddr*>(&address), size) == 0)
      accepted = accept(listener, reinterpret_cast<sockaddr*>(&address), &size);
  }
  int saved = errno;
  close(listener);
  if (accepted < 0) {
    if (client >= 0) close(client);
    errno = saved;
    return -1;
  }
  fds[0] = accepted;
  fds[1] = client;
  return 0;
}
