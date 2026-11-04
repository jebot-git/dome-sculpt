// SPDX-License-Identifier: GPL-3.0-or-later
#include "io.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>

namespace fault {
thread_local bool active = false;
thread_local size_t calls = 0, limit = SIZE_MAX;
void visit() { if (active && calls++ >= limit) throw std::bad_alloc(); }
struct Guard {
  explicit Guard(size_t at) { calls = 0; limit = at; active = true; }
  ~Guard() { active = false; }
};
}
void *operator new(size_t n) {
  fault::visit();
  if (void *p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }

namespace {
size_t checks = 0;
void check(bool b, const char *s) {
  if (!b) throw std::runtime_error(s);
  ++checks;
  std::cout << "PASS " << s << std::endl;
}
std::vector<uint8_t> read(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
void seed(const std::filesystem::path &p) {
  std::ofstream f(p, std::ios::binary);
  f << "Original durable output";
}
size_t descriptors() {
  size_t count = 0;
  for ([[maybe_unused]] const auto &p : std::filesystem::directory_iterator("/proc/self/fd")) ++count;
  return count;
}
}
int main(int argc, char **argv) try {
  if (argc != 2) return 2;
  auto directory = std::filesystem::path(argv[1]) / std::string(96, 'p');
  std::filesystem::create_directories(directory);
  auto path = directory / "actual-output.bin";
  std::vector<uint8_t> payload(1024 * 1024 + 73, 0x5a);
  seed(path);
  const auto prior = read(path);
  const auto initial_descriptors = descriptors();
  size_t baseline = 0, at_commit = 0;
  bool claimed = false;
  dome::AtomicWriteOptions options;
  options.before_commit = [&] { claimed = true; at_commit = fault::calls; };
  {
    fault::Guard guard(SIZE_MAX);
    dome::atomic_write(path, payload, options);
    baseline = fault::calls;
  }
  std::cout << "BASELINE_ALLOCATIONS " << baseline << " COMMIT_BOUNDARY " << at_commit
            << " POST_BOUNDARY " << baseline - at_commit << std::endl;
  bool succeeded = false;
  for (size_t prefix = 0; prefix <= baseline + 32; ++prefix) {
    seed(path);
    claimed = false;
    bool failed = false;
    {
      fault::Guard guard(prefix);
      try { dome::atomic_write(path, payload, options); }
      catch (const std::bad_alloc &) { failed = true; }
    }
    if (failed) {
      if (read(path) != prior) {
        std::cerr << "FAILED_PREFIX " << prefix << " PUBLISHED_NEW_OUTPUT claimed=" << claimed << std::endl;
        return 1;
      }
      check(read(path) == prior, "Persistent allocation denial preserves the exact old file");
    } else {
      check(read(path) == payload && claimed, "Successful retry publishes the complete durable output");
      succeeded = true;
      break;
    }
  }
  check(succeeded && baseline == at_commit, "Successful commit boundary has no subsequent C++ allocation");
  for (size_t cancel_poll : {size_t(3), size_t(5)}) {
    seed(path);
    size_t polls = 0;
    options.cancelled = [&] { return ++polls == cancel_poll; };
    claimed = false;
    bool cancelled = false;
    try { dome::atomic_write(path, payload, options); }
    catch (const std::runtime_error &e) { cancelled = std::string_view(e.what()).find("cancelled") != std::string_view::npos; }
    check(cancelled && !claimed && read(path) == prior,
          "Cancellation during write or immediately before claim preserves old output");
  }
  options.cancelled = {};
  options.before_commit = [] { throw std::bad_alloc(); };
  seed(path);
  bool callback_failed = false;
  try { dome::atomic_write(path, payload, options); }
  catch (const std::bad_alloc &) { callback_failed = true; }
  check(callback_failed && read(path) == prior, "Commit callback allocation failure preserves exact old output");
  size_t temporaries = 0;
  for (const auto &p : std::filesystem::directory_iterator(directory))
    if (p.path().filename().string().find(".tmp-") != std::string::npos) ++temporaries;
  check(temporaries == 0, "Allocation and cancellation refusals leave no temporary outputs");
  check(descriptors() == initial_descriptors, "Successful writes and refusals close every file and directory descriptor");
  std::cout << "PASS " << checks << " atomic allocation checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
