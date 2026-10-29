// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/recovery.hpp"
#include "io.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <iomanip>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
namespace dome {
namespace {
using Json = nlohmann::json;
constexpr uint64_t MaxSceneBytes = 256ull * 1024 * 1024;
struct Fd {
  int value = -1;
  explicit Fd(int v) : value(v) {}
  ~Fd() { if (value >= 0) close(value); }
  Fd(const Fd &) = delete;
};
[[noreturn]] void io_error(const std::string &what) {
  throw std::runtime_error(what + ": " + std::error_code(errno, std::generic_category()).message());
}
void options_valid(const RecoveryOptions &o) {
  if (!o.max_pending || o.max_pending > 8 || !o.retained_per_source ||
      o.retained_per_source > 8 || !o.max_scan_entries || o.max_scan_entries > 16384 ||
      !o.max_file_bytes || o.max_file_bytes > MaxSceneBytes)
    throw std::invalid_argument("Invalid recovery storage limits");
}
bool id_valid(const std::string &id) {
  return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
  });
}
void source_valid(const RecoverySource &s) {
  auto path = s.source_path.string();
  if (!id_valid(s.id) || !s.generation || path.size() > 4096 || path.find('\0') != std::string::npos)
    throw std::invalid_argument("Invalid recovery source identity");
}
void sync_directory(const std::filesystem::path &root);
void private_root(const std::filesystem::path &root, bool create) {
  if (create) {
    std::vector<std::filesystem::path> new_parents;
    for (auto parent = root.parent_path(); !parent.empty() && !std::filesystem::exists(parent); parent = parent.parent_path())
      new_parents.push_back(parent);
    std::filesystem::create_directories(root.parent_path());
    bool created = mkdir(root.c_str(), 0700) == 0;
    if (!created && errno != EEXIST) io_error("Cannot create private recovery directory");
    if (created) sync_directory(root.parent_path());
    for (const auto &parent : new_parents) sync_directory(parent.parent_path());
  }
  struct stat st{};
  if (lstat(root.c_str(), &st)) io_error("Cannot inspect recovery directory");
  if (!S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 0077))
    throw std::runtime_error("Recovery directory must be a private owned directory (0700), not a symlink");
}
void sync_directory(const std::filesystem::path &root) {
  Fd fd(open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (fd.value < 0 || fsync(fd.value)) io_error("Cannot sync recovery directory");
}
struct CheckedFile { uint64_t size = 0, hash = 14695981039346656037ull; std::vector<uint8_t> bytes; };
CheckedFile read_checked(const std::filesystem::path &path, uint64_t limit, bool keep = false) {
  Fd fd(open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (fd.value < 0) io_error("Cannot open recovery file");
  struct stat st{};
  if (fstat(fd.value, &st)) io_error("Cannot inspect recovery file");
  if (!S_ISREG(st.st_mode) || st.st_uid != getuid() || st.st_nlink != 1 || (st.st_mode & 0077))
    throw std::runtime_error("Recovery file must be private, owned, regular and unlinked elsewhere");
  if (st.st_size <= 0 || uint64_t(st.st_size) > limit)
    throw std::length_error("Recovery file exceeds size limit or is empty");
  CheckedFile result;
  if (keep) result.bytes.reserve(size_t(st.st_size));
  std::array<uint8_t, 65536> buffer{};
  for (;;) {
    auto n = read(fd.value, buffer.data(), buffer.size());
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) io_error("Cannot read recovery file");
    if (!n) break;
    result.size += uint64_t(n);
    if (result.size > limit) throw std::length_error("Recovery file grew beyond limit");
    for (ssize_t i = 0; i < n; ++i) result.hash = (result.hash ^ buffer[size_t(i)]) * 1099511628211ull;
    if (keep) result.bytes.insert(result.bytes.end(), buffer.begin(), buffer.begin() + n);
  }
  if (result.size != uint64_t(st.st_size)) throw std::runtime_error("Recovery file changed while reading");
  return result;
}
std::string checksum(uint64_t hash) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}
bool newer(const RecoveryCandidate &a, const RecoveryCandidate &b) {
  return std::tie(a.source.generation, a.revision, a.sequence, a.metadata) >
         std::tie(b.source.generation, b.revision, b.sequence, b.metadata);
}
RecoveryCandidate candidate(const std::filesystem::path &root,
                            const std::filesystem::path &filename, const RecoveryOptions &o) {
  auto name = filename.string();
  if (filename != filename.filename() || filename.extension() != ".recovery" || name.size() > 128)
    throw std::invalid_argument("Invalid recovery metadata filename");
  auto raw = read_checked(root / filename, 16384, true);
  auto j = Json::parse(raw.bytes);
  if (j.at("format") != "dome-sculpt-recovery" || j.at("version") != 1)
    throw std::runtime_error("Unsupported recovery metadata");
  RecoveryCandidate result;
  result.root = root;
  result.metadata = filename;
  result.source = {j.at("source_id").get<std::string>(), j.at("generation").get<uint64_t>(),
                   j.at("source_path").get<std::string>()};
  source_valid(result.source);
  if (!name.starts_with(result.source.id + "-")) throw std::runtime_error("Recovery source/filename mismatch");
  result.revision = j.at("revision").get<uint64_t>();
  result.content = j.at("content").get<uint64_t>();
  result.sequence = j.at("sequence").get<uint64_t>();
  result.bytes = j.at("bytes").get<uint64_t>();
  result.checksum = j.at("fnv1a64").get<std::string>();
  if (!result.revision || !result.sequence || !result.bytes || result.bytes > o.max_file_bytes || result.checksum.size() != 16)
    throw std::runtime_error("Invalid recovery metadata bounds");
  auto data = root / filename;
  data.replace_extension(".dsculpt");
  auto checked = read_checked(data, o.max_file_bytes);
  if (checked.size != result.bytes || checksum(checked.hash) != result.checksum)
    throw std::runtime_error("Recovery scene checksum mismatch");
  return result;
}
struct Scan { std::vector<RecoveryCandidate> records; std::vector<std::string> errors; bool truncated = false; };
Scan scan(const std::filesystem::path &root, const RecoveryOptions &o, const std::string &only_id = {}) {
  Scan result;
  if (!std::filesystem::exists(std::filesystem::symlink_status(root))) return result;
  private_root(root, false);
  size_t visited = 0;
  for (auto &entry : std::filesystem::directory_iterator(root)) {
    if (++visited > o.max_scan_entries) { result.truncated = true; break; }
    auto filename = entry.path().filename();
    if (filename.extension() != ".recovery" || (!only_id.empty() && !filename.string().starts_with(only_id + "-"))) continue;
    try { result.records.push_back(candidate(root, filename, o)); }
    catch (const std::exception &e) { result.errors.push_back(filename.string() + ": " + e.what()); }
  }
  std::sort(result.records.begin(), result.records.end(), newer);
  return result;
}
void remove_record(const RecoveryCandidate &c) {
  // Metadata first: a crash during deletion can only leave ignored orphan data.
  std::filesystem::remove(c.root / c.metadata);
  auto data = c.root / c.metadata;
  data.replace_extension(".dsculpt");
  std::filesystem::remove(data);
}
} // namespace
std::string new_recovery_id() {
  std::array<uint8_t, 16> bytes{};
  size_t done = 0;
  while (done < bytes.size()) {
    auto n = getrandom(bytes.data() + done, bytes.size() - done, 0);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) io_error("Cannot create recovery identity");
    done += size_t(n);
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (auto b : bytes) out << std::setw(2) << unsigned(b);
  return out.str();
}
std::filesystem::path default_recovery_directory() {
  if (auto *state = std::getenv("XDG_STATE_HOME"); state && *state && std::filesystem::path(state).is_absolute())
    return std::filesystem::path(state) / "dome-sculpt/recovery";
  if (auto *home = std::getenv("HOME"); home && *home && std::filesystem::path(home).is_absolute())
    return std::filesystem::path(home) / ".local/state/dome-sculpt/recovery";
  throw std::runtime_error("No absolute user state directory is available");
}
struct RecoveryService::Impl {
  struct Job { RecoverySource source; uint64_t epoch = 0, revision = 0; std::optional<RecoveryRequest> request; };
  std::filesystem::path root;
  RecoveryOptions options;
  mutable std::mutex mutex;
  std::mutex shutdown_mutex;
  std::condition_variable changed;
  RecoverySource source;
  uint64_t epoch = 0, accepted_revision = 0, committed_revision = 0, discard_revision = 0, sequence = 0, active_epoch = 0, active_revision = 0;
  std::deque<Job> queue;
  RecoveryStatus state;
  std::thread worker;
  Impl(std::filesystem::path path, RecoveryOptions opts) : root(std::filesystem::absolute(path).lexically_normal()), options(std::move(opts)) {
    options_valid(options);
    worker = std::thread([this] { run(); });
  }
  bool current(const Job &job) const { return job.epoch == epoch && job.source == source; }
  void checkpoint(RecoveryPhase phase) { if (options.checkpoint) options.checkpoint(phase); }
  void prune(const std::string &id, std::optional<std::pair<uint64_t,uint64_t>> through = {}) {
    checkpoint(RecoveryPhase::Prune);
    auto found = scan(root, options, id);
    if (found.truncated) throw std::runtime_error("Recovery directory scan limit reached; old checkpoints retained");
    size_t kept = 0;
    for (const auto &record : found.records) if (record.source.id == id) {
      bool covered = through && (record.source.generation < through->first ||
          (record.source.generation == through->first && record.revision <= through->second));
      if (covered || (!through && ++kept > options.retained_per_source)) remove_record(record);
    }
    sync_directory(root);
  }
  void persist(const Job &job) {
    const auto &request = *job.request;
    private_root(root, true);
    // Concurrent application instances use distinct random IDs. Lock this ID
    // while publishing/pruning as an additional guard against accidental reuse.
    Fd owner(open((root / (job.source.id + ".lock")).c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (owner.value < 0 || flock(owner.value, LOCK_EX | LOCK_NB)) io_error("Recovery source is already being persisted");
    auto previous = scan(root, options, job.source.id);
    if (previous.truncated) throw std::runtime_error("Recovery scan limit reached; previous checkpoints retained");
    if (!previous.records.empty()) {
      const auto &latest = previous.records.front();
      if (latest.source.generation > job.source.generation || (latest.source.generation == job.source.generation && latest.revision >= request.revision)) {
        std::lock_guard lock(mutex); ++state.stale;
        if (current(job)) { state.latest = latest; state.error = "A newer recovery already exists for this source"; }
        return;
      }
    }
    auto token = job.source.id + "-" + new_recovery_id();
    auto staging = root / (token + ".pending"), data = root / (token + ".dsculpt");
    auto meta_staging = root / (token + ".metadata-pending"), meta = root / (token + ".recovery");
    bool published = false;
    try {
      checkpoint(RecoveryPhase::Serialize);
      request.snapshot.save(staging); // Trusted native serializer/atomic fsync.
      auto checked = read_checked(staging, options.max_file_bytes);
      auto now = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
      sequence = std::max(sequence + 1, now);
      RecoveryCandidate record{request.source, request.revision, request.content, sequence,
          checked.size, checksum(checked.hash), root, meta.filename()};
      Json metadata = {{"format", "dome-sculpt-recovery"}, {"version", 1},
          {"source_id", record.source.id}, {"generation", record.source.generation},
          {"source_path", record.source.source_path.string()}, {"revision", record.revision},
          {"content", record.content}, {"sequence", record.sequence}, {"bytes", record.bytes},
          {"fnv1a64", record.checksum}};
      auto text = metadata.dump(2) + '\n';
      atomic_write(meta_staging, std::span(reinterpret_cast<const uint8_t *>(text.data()), text.size()));
      std::filesystem::rename(staging, data);
      checkpoint(RecoveryPhase::Commit);
      {
        std::lock_guard lock(mutex);
        if (!current(job) || request.revision <= discard_revision || request.revision <= committed_revision) {
          ++state.stale;
        } else {
          // Only this tiny publication step holds the source-order mutex. Bulk
          // serialization, file fsync, hashing and directory sync never do.
          std::filesystem::rename(meta_staging, meta);
          published = true;
        }
      }
      if (!published) {
        std::filesystem::remove(data);
        std::filesystem::remove(meta_staging);
        return;
      }
      // Do not prune the previous checkpoint unless publication is durable.
      sync_directory(root);
      {
        std::lock_guard lock(mutex);
        ++state.committed;
        if (current(job)) { committed_revision = request.revision; state.latest = record; state.error.clear(); }
      }
      prune(job.source.id);
    } catch (...) {
      // Once published, retain both records even if directory sync/prune failed.
      // Discovery can verify/fall back; errors are surfaced rather than hidden.
      std::error_code ignored;
      std::filesystem::remove(staging, ignored);
      std::filesystem::remove(meta_staging, ignored);
      if (!published) std::filesystem::remove(data, ignored);
      throw;
    }
  }
  void run() {
    for (;;) {
      Job job;
      {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return state.stopping || !queue.empty(); });
        if (queue.empty()) { if (state.stopping) return; continue; }
        job = std::move(queue.front()); queue.pop_front(); state.busy = true; active_epoch = job.epoch; active_revision = job.request ? job.revision : 0;
      }
      try {
        bool valid;
        { std::lock_guard lock(mutex); valid = current(job); if (!valid) ++state.stale; }
        if (valid) {
          if (job.request) persist(job);
          else {
            private_root(root, true);
            prune(job.source.id, {{job.source.generation, job.revision}});
            std::lock_guard lock(mutex);
            if (current(job)) { if (state.latest && state.latest->revision <= job.revision) state.latest.reset(); state.error.clear(); }
          }
        }
      } catch (const std::exception &e) {
        std::lock_guard lock(mutex); ++state.failed; state.error = e.what();
      } catch (...) {
        std::lock_guard lock(mutex); ++state.failed; state.error = "Unknown recovery persistence failure";
      }
      { std::lock_guard lock(mutex); state.busy = false; active_revision = 0; }
      changed.notify_all();
    }
  }
};
RecoveryService::RecoveryService(std::filesystem::path root, RecoveryOptions options)
    : impl_(std::make_unique<Impl>(std::move(root), std::move(options))) {}
RecoveryService::~RecoveryService() { shutdown(); }
bool RecoveryService::set_source(RecoverySource source) {
  source_valid(source);
  std::lock_guard lock(impl_->mutex);
  if (impl_->state.stopping || (source.id == impl_->source.id && source.generation < impl_->source.generation)) return false;
  if (source == impl_->source) return true;
  bool same_generation = source.id == impl_->source.id && source.generation == impl_->source.generation;
  impl_->source = std::move(source); ++impl_->epoch;
  impl_->state.stale += impl_->queue.size(); impl_->queue.clear(); impl_->state.latest.reset();
  if (!same_generation) impl_->accepted_revision = impl_->committed_revision = impl_->discard_revision = 0;
  return true;
}
RecoveryEnqueue RecoveryService::enqueue(RecoveryRequest request) {
  source_valid(request.source);
  if (!request.revision) throw std::invalid_argument("Recovery revision must be positive");
  std::lock_guard lock(impl_->mutex);
  if (impl_->state.stopping) return RecoveryEnqueue::Stopped;
  bool same_pending = (impl_->state.busy && impl_->active_epoch == impl_->epoch && impl_->active_revision == request.revision) || std::any_of(impl_->queue.begin(), impl_->queue.end(), [&](const auto &job) { return job.request && job.revision == request.revision; });
  if (request.source != impl_->source || request.revision < impl_->accepted_revision || request.revision <= impl_->committed_revision || request.revision <= impl_->discard_revision || same_pending) {
    ++impl_->state.stale; return RecoveryEnqueue::Stale;
  }
  bool coalesced = false;
  size_t pending = std::count_if(impl_->queue.begin(), impl_->queue.end(), [](const auto &job) { return job.request.has_value(); });
  if (pending >= impl_->options.max_pending) {
    auto oldest = std::find_if(impl_->queue.begin(), impl_->queue.end(), [](const auto &job) { return job.request.has_value(); });
    impl_->queue.erase(oldest); ++impl_->state.coalesced; coalesced = true;
  }
  impl_->accepted_revision = request.revision; ++impl_->state.accepted;
  auto source = request.source; auto revision = request.revision;
  impl_->queue.push_back({std::move(source), impl_->epoch, revision, std::move(request)});
  impl_->changed.notify_all();
  return coalesced ? RecoveryEnqueue::Coalesced : RecoveryEnqueue::Accepted;
}
bool RecoveryService::discard(const RecoverySource &source, uint64_t through_revision) {
  std::lock_guard lock(impl_->mutex);
  if (impl_->state.stopping || source != impl_->source) return false;
  impl_->discard_revision = std::max(impl_->discard_revision, through_revision);
  std::erase_if(impl_->queue, [&](const auto &job) { return !job.request || job.revision <= through_revision; });
  impl_->queue.push_back({source, impl_->epoch, impl_->discard_revision, {}});
  impl_->changed.notify_all(); return true;
}
bool RecoveryService::flush(std::chrono::milliseconds timeout) {
  std::unique_lock lock(impl_->mutex);
  return impl_->changed.wait_for(lock, timeout, [&] { return impl_->queue.empty() && !impl_->state.busy; });
}
RecoveryStatus RecoveryService::status() const {
  std::lock_guard lock(impl_->mutex); auto result = impl_->state; result.pending = impl_->queue.size(); return result;
}
void RecoveryService::shutdown() {
  std::lock_guard shutdown(impl_->shutdown_mutex);
  { std::lock_guard lock(impl_->mutex); impl_->state.stopping = true; }
  impl_->changed.notify_all();
  if (impl_->worker.joinable()) impl_->worker.join();
}
RecoveryDiscovery RecoveryService::discover(const std::filesystem::path &root, RecoveryOptions options) {
  options_valid(options); RecoveryDiscovery result;
  try {
    auto found = scan(std::filesystem::absolute(root).lexically_normal(), options);
    result.errors = std::move(found.errors); result.truncated = found.truncated;
    std::map<std::string, bool> ids;
    for (auto &record : found.records) if (ids.emplace(record.source.id, true).second) result.candidates.push_back(std::move(record));
    if (result.truncated) result.errors.push_back("Recovery discovery scan limit reached");
  } catch (const std::exception &e) { result.errors.push_back(e.what()); }
  return result;
}
Scene RecoveryService::load(const RecoveryCandidate &selected, RecoveryOptions options) {
  options_valid(options); private_root(selected.root, false);
  auto record = candidate(selected.root, selected.metadata, options);
  if (record.source != selected.source || record.revision != selected.revision || record.content != selected.content ||
      record.sequence != selected.sequence || record.checksum != selected.checksum || record.bytes != selected.bytes)
    throw std::runtime_error("Selected recovery changed; refresh discovery before opening it");
  auto data = selected.root / selected.metadata; data.replace_extension(".dsculpt");
  auto checked = read_checked(data, options.max_file_bytes, true);
  if (checked.size != record.bytes || checksum(checked.hash) != record.checksum)
    throw std::runtime_error("Recovery scene changed while loading");
  return Scene::decode(Json::from_cbor(checked.bytes));
}
} // namespace dome
