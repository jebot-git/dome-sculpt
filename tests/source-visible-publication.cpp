// SPDX-License-Identifier: GPL-3.0-or-later
#include "document-authority.hpp"
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include "dome/rig-rest.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
std::atomic_bool enabled{false}, hit{false};
std::atomic_size_t calls{0}, boundary{SIZE_MAX};
thread_local bool tracking = false;
std::atomic<Document *> current{nullptr};
void require(bool v, const char *why) {
  if (!v)
    throw std::runtime_error(why);
}
struct Gate {
  std::mutex mutex;
  std::condition_variable changed;
  bool armed = false, entered = false;
  void arm() {
    std::lock_guard l(mutex);
    armed = true;
    entered = false;
  }
  void stop() {
    std::unique_lock l(mutex);
    if (!armed)
      return;
    entered = true;
    changed.notify_all();
    changed.wait(l, [&] { return !armed; });
  }
  void wait() {
    std::unique_lock l(mutex);
    if (!changed.wait_for(l, std::chrono::seconds(120),
                          [&] { return entered; }))
      throw std::runtime_error("Actual source helper gate missing");
  }
  void release() {
    std::lock_guard l(mutex);
    armed = false;
    changed.notify_all();
  }
} gate;
struct Release {
  ~Release() {
    enabled = false;
    tracking = false;
    gate.release();
  }
};
bool prepare_or_replay(Scene &, std::span<const uint64_t>,
                       const SkinAuthoringOptions &,
                       const std::function<bool()> &,
                       SourceVisibleSkinReport *);
Scene fixture() {
  Scene s;
  append_humanoid(s, HumanoidControls{}, "Publication source", {}, 64);
  auto p =
      std::find_if(s.objects().begin(), s.objects().end(), [](const auto &o) {
        return o.name.find("Body_00_SKIN #3") != std::string::npos;
      });
  require(p != s.objects().end(), "Genuine Body fixture");
  s.select(p->id);
  return s;
}
std::unique_ptr<Document> setup() {
  static const auto base = fixture();
  auto d = std::make_unique<Document>();
  d->replace(base.snapshot());
  current = d.get();
  for (const auto &name : {"Original source A", "Original source B"}) {
    auto o = d->copy_scene().active();
    d->object_properties(o.id, name, o.visible, o.locked, o.transform,
                         o.material);
  }
  require(d->undo() && d->wait_mesh(std::chrono::seconds(120)),
          "Actual prior history/source ready");
  return d;
}
SkinAuthoringOptions options(const Document &d) {
  auto o = d.frame().state->skin_options;
  o.mode = SkinAuthoringMode::SourceVisible;
  o.reference = d.frame().state->active_object;
  return o;
}
} // namespace
[[gnu::noinline]] void *operator new(size_t n) {
  if (tracking && enabled && ++calls >= boundary.load()) {
    hit = true;
    throw std::bad_alloc();
  }
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void *p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void *p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void *p, size_t) noexcept {
  std::free(p);
}
[[gnu::noinline]] void operator delete[](void *p, size_t) noexcept {
  std::free(p);
}
extern "C" bool
source_real(Scene &, std::span<const uint64_t>, const SkinAuthoringOptions &,
            const std::function<bool()> &,
            SourceVisibleSkinReport
                *) asm("__real__ZN4dome26author_source_visible_skinERNS_"
                       "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                       "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                       "23SourceVisibleSkinReportE");
extern "C" bool
source_wrap(Scene &, std::span<const uint64_t>, const SkinAuthoringOptions &,
            const std::function<bool()> &,
            SourceVisibleSkinReport
                *) asm("__wrap__ZN4dome26author_source_visible_skinERNS_"
                       "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                       "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                       "23SourceVisibleSkinReportE");
extern "C" bool source_wrap(Scene &s, std::span<const uint64_t> ids,
                            const SkinAuthoringOptions &o,
                            const std::function<bool()> &cancel,
                            SourceVisibleSkinReport *r) {
  tracking = false;
  gate.stop();
  auto changed = prepare_or_replay(s, ids, o, cancel, r);
  tracking = enabled.load();
  return changed;
}
extern "C" void
commit_real(void *, bool) asm("__real__ZN4dome8Document11Transaction6commitEb");
extern "C" void
commit_wrap(void *, bool) asm("__wrap__ZN4dome8Document11Transaction6commitEb");
extern "C" void commit_wrap(void *self, bool checkpoint) {
  tracking = false;
  commit_real(self, checkpoint);
}
extern "C" void error_real(Document *, void *, std::exception_ptr) asm(
    "__real__ZN4dome8Document24file_worker_error_lockedERNS0_7FileJobENSt15__"
    "exception_ptr13exception_ptrE");
extern "C" void error_wrap(Document *, void *, std::exception_ptr) asm(
    "__wrap__ZN4dome8Document24file_worker_error_lockedERNS0_7FileJobENSt15__"
    "exception_ptr13exception_ptrE");
extern "C" void error_wrap(Document *d, void *j, std::exception_ptr e) {
  error_real(d, j, std::move(e));
  tracking = false;
}
namespace dome {
struct DocumentTransactionTestAccess {
  static auto tokens(const Document &d) {
    std::lock_guard l(d.model_mutex_);
    return std::tuple{
        d.content_,           d.next_content_,          d.saved_content_,
        d.undo_.size(),       d.redo_.size(),           d.state_.revision,
        d.state_.generation,  d.recovery_content_,      d.state_.edit_authority,
        d.state_.held_object, d.state_.selected_objects};
  }
  static auto admitted(const Document &d) {
    std::lock_guard l(d.model_mutex_);
    return std::tuple{d.state_.generation, d.state_.revision,
                      d.scene_.active().id, d.state_.edit_authority};
  }
  static auto work(const Document &d) { return d.asset_work_.load(); }
};
} // namespace dome
namespace {
auto option_key(const SkinAuthoringOptions &o) {
  return std::tuple{o.reference,
                    o.mode,
                    o.limits.max_influences,
                    std::bit_cast<uint32_t>(o.limits.prune),
                    std::bit_cast<uint32_t>(o.seed_distance),
                    std::bit_cast<uint32_t>(o.seed_margin),
                    std::bit_cast<uint32_t>(o.falloff),
                    std::bit_cast<uint32_t>(o.transfer_distance),
                    o.maximum_work,
                    o.locks,
                    o.source_visible.maximum_work,
                    o.source_visible.maximum_workspace};
}
struct Cache {
  std::shared_ptr<const Scene> input, output, witness;
  nlohmann::json input_value, output_value;
  SkinAuthoringOptions options;
  std::vector<uint64_t> targets;
  std::tuple<uint64_t, uint64_t, uint64_t, uint64_t> admitted;
  bool changed = false;
  SourceVisibleSkinReport report;
} cache;
void paint_owners(const PaintedMesh &a, const PaintedMesh &b) {
  require(a.mesh_owner() == b.mesh_owner() && a.revision() == b.revision() &&
              a.active_layer() == b.active_layer() &&
              a.resolution() == b.resolution() &&
              a.surface_diagnostics().policy ==
                  b.surface_diagnostics().policy &&
              a.baked() == b.baked() && a.source_maps() == b.source_maps() &&
              a.color_projection() == b.color_projection() &&
              a.normal_projection() == b.normal_projection() &&
              a.paint_sampling_domain() == b.paint_sampling_domain() &&
              a.paint_sampling_attachment() == b.paint_sampling_attachment() &&
              a.layers().size() == b.layers().size(),
          "Replay authenticates exact immutable paint/geometry/basis/recipe "
          "owners");
  for (size_t l = 0; l < a.layers().size(); ++l) {
    const auto &x = a.layers()[l];
    const auto &y = b.layers()[l];
    require(x.id == y.id && x.name == y.name && x.visible == y.visible &&
                std::bit_cast<uint32_t>(x.opacity) ==
                    std::bit_cast<uint32_t>(y.opacity) &&
                x.tiles == y.tiles && x.pbr_tiles == y.pbr_tiles &&
                x.emission_tiles == y.emission_tiles,
            "Replay preserves complete layer state and all common/PBR/emission "
            "tile owners");
  }
}
void unchanged() {
  require(cache.input->encode() == cache.input_value &&
              cache.output->encode() == cache.output_value &&
              cache.witness->encode() == cache.output_value,
          "Independent cached complete inputs/outputs remain exact");
  detail::require_isolated_scene(*cache.witness, *cache.output, 0);
  for (size_t i = 0; i < cache.output->objects().size(); ++i) {
    auto &a = cache.witness->objects()[i];
    auto &b = cache.output->objects()[i];
    require(a.volume != b.volume && a.volume->encode() == b.volume->encode(),
            "Cached volume metadata remains independently owned");
    if (a.painted) {
      require(a.painted != b.painted,
              "Cached paint metadata has independent witness");
      paint_owners(*a.painted, *b.painted);
    }
  }
}
bool prepare_or_replay(Scene &s, std::span<const uint64_t> ids,
                       const SkinAuthoringOptions &o,
                       const std::function<bool()> &cancel,
                       SourceVisibleSkinReport *r) {
  auto *d = current.load();
  require(d, "Actual native Document exists");
  if (!cache.output) {
    cache.input = std::make_shared<const Scene>(s.snapshot());
    cache.input_value = s.encode();
    cache.options = o;
    cache.targets.assign(ids.begin(), ids.end());
    cache.admitted = DocumentTransactionTestAccess::admitted(*d);
    cache.changed = source_real(s, ids, o, cancel, r);
    cache.output = std::make_shared<const Scene>(s.snapshot());
    cache.witness = std::make_shared<const Scene>(cache.output->snapshot());
    cache.output_value = s.encode();
  } else {
    require(s.encode() == cache.input_value &&
                option_key(o) == option_key(cache.options) &&
                std::vector<uint64_t>(ids.begin(), ids.end()) ==
                    cache.targets &&
                DocumentTransactionTestAccess::admitted(*d) == cache.admitted,
            "Replay requires complete input/options/target IDs and actual "
            "generation/revision/object/authority");
    detail::require_isolated_scene(*cache.input, s, 0);
    for (size_t i = 0; i < s.objects().size(); ++i)
      if (s.objects()[i].painted)
        paint_owners(*cache.input->objects()[i].painted,
                     *s.objects()[i].painted);
    unchanged();
    Scene recipient = s;
    for (auto id : ids) {
      auto &out = recipient.object(id);
      const auto &prepared = cache.output->object(id);
      out.deformation = prepared.deformation;
      out.rig_source = prepared.rig_source;
    }
    // Apply only independently authenticated immutable target deltas. Actual
    // recipient paint/volume/material/ghost identities are never overwritten.
    for (const auto &before : s.objects()) {
      const auto &after = recipient.object(before.id);
      require(before.painted == after.painted &&
                  before.volume == after.volume &&
                  detail::same_material(before.material, after.material) &&
                  before.appearance == after.appearance &&
                  before.material_bindings == after.material_bindings,
              "Replay preserves actual recipient mutable "
              "paint/volume/material/source owners");
      if (std::find(ids.begin(), ids.end(), before.id) == ids.end())
        require(detail::same_object_authority(before, after),
                "Replay cannot replace ghost authority");
    }
    recipient.validate();
    require(recipient.encode() == cache.output_value,
            "Authenticated target delta exactly reproduces real preparation");
    s = std::move(recipient);
    unchanged();
    if (r)
      *r = cache.report;
  }
  return cache.changed;
}
std::future<void> accept(Document &d, const std::filesystem::path &path) {
  gate.arm();
  d.bind_skin(options(d));
  gate.wait();
  return d.save_async(path);
}
} // namespace
int main(int argc, char **argv) try {
  require(argc == 2 || argc == 4,
          "Choose publication output and optional inclusive first/last");
  std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  auto measured = setup();
  Release release;
  auto saved = accept(*measured, out / "real-prepared.dsculpt");
  calls = 0;
  boundary = SIZE_MAX;
  hit = false;
  enabled = true;
  gate.release();
  saved.get();
  enabled = false;
  require(measured->wait_asset() && measured->frame().state->error.empty(),
          "Real helper/publication/Save succeeds");
  const auto count = calls.load();
  unchanged();
  auto expected = measured->copy_scene().encode();
  require(count > 100,
          "Measured real post-helper publication allocation count");
  auto restored = Scene::load(out / "real-prepared.dsculpt");
  restore_rig_source(restored, restored.active().id, false);
  require(restored.encode() == cache.input_value,
          "Real saved changed bind restores complete original source");
  export_glb(measured->copy_scene().export_surfaces(),
             out / "real-current.glb");
  require(std::filesystem::file_size(out / "real-current.glb") > 1000,
          "Real preparation baseline emits current standard GLB");
  measured.reset();
  std::cout << "MEASURE post_helper_allocations=" << count << std::endl;
  size_t first = 1, last = count;
  if (argc == 4) {
    first = std::stoull(argv[2]);
    last = std::min(count, size_t(std::stoull(argv[3])));
  }
  require(first && first <= last && last <= count,
          "Actual measured publication range");
  for (size_t at = first; at <= last; ++at) {
    auto d = setup();
    Release cleanup;
    auto input = d->copy_scene().encode();
    auto path = out / "prior.dsculpt";
    d->copy_scene().save(path);
    auto tokens = DocumentTransactionTestAccess::tokens(*d);
    auto mesh = d->frame().surface;
    auto result = accept(*d, path);
    auto work = DocumentTransactionTestAccess::work(*d);
    calls = 0;
    boundary = at;
    hit = false;
    enabled = true;
    gate.release();
    bool failed = false;
    try {
      result.get();
    } catch (const std::bad_alloc &) {
      failed = true;
    }
    enabled = false;
    require(d->wait_asset() && hit && failed,
            "Actual post-helper allocation denial and original Save failure");
    unchanged();
    require(d->copy_scene().encode() == input &&
                Scene::load(path).encode() == input &&
                DocumentTransactionTestAccess::tokens(*d) == tokens &&
                d->frame().surface == mesh,
            "Failed publication preserves complete Scene/prior "
            "file/history/content/revision/generation/authority/mesh");
    require(work->done && work->failure && !work->completed,
            "Persistent-denial accepted endpoint failure is exact");
    auto retry = accept(*d, path);
    gate.release();
    retry.get();
    require(d->wait_asset() && d->frame().state->error.empty() &&
                d->copy_scene().encode() == expected,
            "Same actual native admission retries authenticated preparation");
    require(d->undo() && d->copy_scene().encode() == input && d->redo() &&
                d->copy_scene().encode() == expected,
            "Actual retry adds exactly one source-retaining Undo/Redo");
    unchanged();
    if ((at & 63) == 0)
      std::cout << "PUBLICATION_PASS " << at << '/' << count << std::endl;
  }
  std::cout << "PASS exhaustive selected range=" << first << ".." << last
            << " total=" << count
            << " cached_preparation=1 recipient_identity=1 save=1 history=1 "
               "restore=1\n";
  return 0;
} catch (const std::exception &e) {
  enabled = false;
  tracking = false;
  gate.release();
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
