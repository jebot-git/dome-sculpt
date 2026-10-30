// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only observation of the real FLTK status label. No application hooks or
// replacement event path: forward every label to the canonical shared toolkit.
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
namespace {
constexpr char expected[] =
    "Sculpt ray exceeds the bounded grid traversal budget at this resolution";
}
extern "C" void
observe_label(void *, const char *) asm("_ZN9Fl_Widget10copy_labelEPKc");
extern "C" void observe_label(void *widget, const char *label) {
  using CopyLabel = void (*)(void *, const char *);
  static const auto original = reinterpret_cast<CopyLabel>(
      dlsym(RTLD_NEXT, "_ZN9Fl_Widget10copy_labelEPKc"));
  if (!original)
    std::abort();
  original(widget, label);
  const char *path = std::getenv("DOME_SCULPT_STATUS_ACK");
  if (!path || !label || !std::strstr(label, expected))
    return;
  const int fd =
      open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (fd < 0)
    return;
  const auto ignored = write(fd, label, strnlen(label, 2048));
  (void)ignored;
  close(fd);
}
