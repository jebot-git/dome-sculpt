// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace dome {
struct RecoverySource {
  std::string id;
  uint64_t generation = 0;
  std::filesystem::path source_path; // Display metadata; never written/opened.
  bool operator==(const RecoverySource &) const = default;
};
struct RecoveryRequest {
  RecoverySource source;
  uint64_t revision = 0, content = 0;
  Scene snapshot; // Capture Scene::snapshot() under the document model lock.
};
struct RecoveryCandidate {
  RecoverySource source;
  uint64_t revision = 0, content = 0, sequence = 0, bytes = 0;
  std::string checksum;
  std::filesystem::path root, metadata;
};
struct RecoveryDiscovery {
  std::vector<RecoveryCandidate> candidates; // Newest valid record per source.
  std::vector<std::string> errors;           // Corrupt newer records stay visible.
  bool truncated = false;
};
enum class RecoveryPhase { Serialize, Commit, Prune };
struct RecoveryOptions {
  size_t max_pending = 2; // In addition to at most one in-flight immutable scene.
  size_t retained_per_source = 2;
  size_t max_scan_entries = 1024;
  uint64_t max_file_bytes = 256ull * 1024 * 1024;
  // Optional storage instrumentation/fault injection, called only on the worker.
  // Exceptions become a reported persistence failure; never call UI/model code.
  std::function<void(RecoveryPhase)> checkpoint;
};
enum class RecoveryEnqueue { Accepted, Coalesced, Stale, Stopped };
struct RecoveryStatus {
  size_t pending = 0;
  bool busy = false, stopping = false;
  uint64_t accepted = 0, coalesced = 0, stale = 0, committed = 0, failed = 0;
  std::optional<RecoveryCandidate> latest;
  std::string error; // Sticky until a later successful operation; flush != success.
};
std::string new_recovery_id();
std::filesystem::path default_recovery_directory();

// Single ordered worker, bounded/coalesced pending scene snapshots. Constructors
// do not touch disk. No callback, serialization, hashing, scan or file operation
// runs in enqueue/set_source/status. Destruction drains accepted work and joins.
// Source IDs must uniquely identify a logical document, not a filename basename.
class RecoveryService {
public:
  explicit RecoveryService(std::filesystem::path root,
                           RecoveryOptions options = {});
  ~RecoveryService();
  RecoveryService(const RecoveryService &) = delete;
  RecoveryService &operator=(const RecoveryService &) = delete;
  // Replacement invalidates queued/in-flight old source jobs. A lower generation
  // for the same ID is rejected. Metadata alone cannot overwrite a newer source.
  bool set_source(RecoverySource);
  RecoveryEnqueue enqueue(RecoveryRequest);
  // Call only after a successful durable user save or explicit discard. Deletes
  // only this source's checkpoints through the saved revision, retaining newer
  // unsaved edits. Old save completions cannot clear a replacement document.
  bool discard(const RecoverySource &, uint64_t through_revision);
  bool flush(std::chrono::milliseconds timeout = std::chrono::seconds(30));
  RecoveryStatus status() const;
  void shutdown(); // Drain and stop; safe to call repeatedly.

  // File-worker/startup operations, never an XR frame callback. Discovery checks
  // file size and checksum and returns the previous valid record on corruption.
  static RecoveryDiscovery discover(const std::filesystem::path &root,
                                    RecoveryOptions options = {});
  static Scene load(const RecoveryCandidate &, RecoveryOptions options = {});

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace dome
