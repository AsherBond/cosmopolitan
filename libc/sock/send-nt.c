/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/internal.h"
#include "libc/calls/sig.internal.h"
#include "libc/calls/struct/iovec.internal.h"
#include "libc/calls/struct/sigset.internal.h"
#include "libc/errno.h"
#include "libc/nt/errors.h"
#include "libc/nt/struct/iovec.h"
#include "libc/nt/struct/overlapped.h"
#include "libc/nt/winsock.h"
#include "libc/sock/internal.h"
#include "libc/sysv/consts/sicode.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/errfuns.h"
#include "libc/vga/vga.internal.h"
#ifdef __x86_64__

#define _MSG_OOB       1
#define _MSG_DONTROUTE 4
#define _MSG_DONTWAIT  64
#define _MSG_NOSIGNAL  0x10000000

struct SendArgs {
  const struct iovec *iov;
  size_t iovlen;
  struct NtIovec iovnt[16];
};

textwindows static int sys_send_nt_start(int64_t handle,
                                         struct NtOverlapped *overlap,
                                         uint32_t *flags, void *arg) {
  struct SendArgs *args = arg;
  return WSASend(handle, args->iovnt,
                 __iovec2nt(args->iovnt, args->iov, args->iovlen), 0, *flags,
                 overlap, 0);
}

textwindows ssize_t sys_send_nt(int fd, const struct iovec *iov, size_t iovlen,
                                uint32_t flags) {
  if (flags & ~(_MSG_DONTWAIT | _MSG_OOB | _MSG_DONTROUTE | _MSG_NOSIGNAL))
    return einval();
  ssize_t rc;
  struct Fd *f = g_fds.p + fd;
  sigset_t waitmask = __sig_block();

  rc = __winsock_block(f->handle, flags & ~(_MSG_DONTWAIT | _MSG_NOSIGNAL),
                       false, f->sndtimeo, waitmask, sys_send_nt_start,
                       &(struct SendArgs){iov, iovlen});

  __sig_unblock(waitmask);

  if (rc == -1 && errno == WSAESHUTDOWN) {  // ESHUTDOWN
    errno = kNtErrorBrokenPipe;             // EPIPE
    if (!(flags & _MSG_NOSIGNAL))
      __sig_raise(SIGPIPE, SI_KERNEL);
  }

  return rc;
}

#endif /* __x86_64__ */
