// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
#include "paint-composite-backend.hpp"
#include "source-uv-sampler-kernel.hpp"

namespace dome::detail {
namespace {
struct Release {
  sycl::queue *queue;
  template<class T> void operator()(T *p) const noexcept {
    if (p) try { sycl::free(p, *queue); } catch (...) {}
  }
};

class SyclSampler final : public SourceUvSampler {
  sycl::queue queue_{sycl::gpu_selector_v, [](sycl::exception_list errors) {
    for (auto error : errors) std::rethrow_exception(error);
  }, sycl::property_list{sycl::property::queue::in_order{}}};
  template<class T> std::unique_ptr<T, Release> allocate(size_t count) {
    std::unique_ptr<T, Release> buffer{sycl::malloc_device<T>(count, queue_), {&queue_}};
    if (!buffer) throw std::bad_alloc();
    return buffer;
  }
  std::unique_ptr<uint8_t, Release> image_;
  std::unique_ptr<paint_compute::Quantization, Release> table_;
  std::unique_ptr<Coordinate, Release> coordinates_;
  std::unique_ptr<uint32_t, Release> colors_;
  MappedSourceImage source_;

public:
  SyclSampler(const PaintSourceTexture &source, bool srgb, Output format, size_t capacity)
      : SourceUvSampler(capacity, format), image_(allocate<uint8_t>(source.image->rgba.size())),
        table_(allocate<paint_compute::Quantization>(1)),
        coordinates_(allocate<Coordinate>(capacity)), colors_(allocate<uint32_t>(capacity * words(format))),
        source_{image_.get(), source.image->width, source.image->height,
                source.wrap_s, source.wrap_t, source.mag_filter == 9728, srgb, format} {
    const auto &table = paint_compute::quantization();
    queue_.memcpy(image_.get(), source.image->rgba.data(), source.image->rgba.size()).wait_and_throw();
    queue_.memcpy(table_.get(), &table, sizeof(table)).wait_and_throw();
  }

private:
  void dispatch(std::span<const Coordinate> pending, std::span<uint32_t> result) override {
    const auto *coordinates = coordinates_.get();
    auto *colors = colors_.get();
    const auto *table = table_.get();
    const auto source = source_;
    try {
      queue_.memcpy(coordinates_.get(), pending.data(), pending.size_bytes());
      queue_.parallel_for(sycl::range<1>{pending.size()}, [=](sycl::id<1> id) {
        sample_mapped_source(source, coordinates[id[0]], *table, colors, id[0]);
      });
      queue_.memcpy(result.data(), colors, result.size_bytes()).wait_and_throw();
    } catch (...) {
      try { queue_.wait_and_throw(); } catch (...) {}
      throw;
    }
  }
};
}

std::unique_ptr<SourceUvSampler> upload_source_uv_sampler(
    const PaintSourceTexture &source, bool srgb, SourceUvSampler::Output format, size_t capacity) {
  return std::make_unique<SyclSampler>(source, srgb, format, capacity);
}
}
