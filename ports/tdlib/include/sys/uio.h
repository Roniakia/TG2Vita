#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
// Nonblocking-compatible scatter write: return progress on partial/error writes.
static inline ssize_t writev(int fd, const struct iovec *iov, int count) {
  if (count < 0 || count > 1024) { errno = EINVAL; return -1; }
  size_t size = 0;
  for (int i = 0; i < count; ++i) {
    if (iov[i].iov_len > (size_t)INT_MAX - size) { errno = EINVAL; return -1; }
    size += iov[i].iov_len;
  }
  ssize_t total = 0;
  for (int i = 0; i < count; ++i) {
    if (!iov[i].iov_len) continue;
    ssize_t n = write(fd, iov[i].iov_base, iov[i].iov_len);
    if (n < 0) return total ? total : n;
    total += n;
    if ((size_t)n < iov[i].iov_len) break;
  }
  return total;
}
