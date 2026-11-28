// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-composite-backend.hpp"
#include "emission-raster-device.hpp"
#include "paint-source-coordinate-kernel.hpp"
#include "paint-grid-filter.hpp"
#include <cstring>
namespace dome::detail::emission_raster {
std::unique_ptr<EmissionRasterDevice> upload(const Input &,const std::function<bool()> &){
 throw std::logic_error("Select a GPU backend for device emission compositing");
}
}
namespace dome::paint_compute {
Result composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &q=quantization();
 Result out(in);
 std::vector<Pixel> borrowed;
 if(!in.native_tiles.empty()){
  borrowed.resize(in.packed_texels());
  visit_native_tile_uploads(in,[&](size_t first,std::span<const std::byte> bytes){
   std::memcpy(borrowed.data()+first,bytes.data(),bytes.size());
  },cancelled);
 }
 const auto *pixels=borrowed.empty()?in.pixels.data():borrowed.data();
 const auto padding=in.padding_view();
 const auto bake=in.bake.view();
 const auto prepared=in.prepared_view(in.projected_color.data(),in.baked_pbr_color.data(),
     in.color_tiles.empty()?nullptr:in.color_tiles.data(),in.color_pixels.data(),in.projected_columns.data(),in.projected_rows.data());
 for(const auto grid:in.output_grids()){
  if(!grid.channels)break;
  auto source=in.source_views();
  SourceCoordinateBatch batch;
  SourcePixelTaps mapped[SourceRoleCount];
  for(size_t role=0;role<in.sources.size();++role){
   const auto &texture=in.sources[role];
   if(!texture.coupled()||!grid.writes(source_channels[role]))continue;
   source.textures[role].mapped=&mapped[batch.size];
   batch.jobs[batch.size]=texture.coordinate_job(texture.column_terms.data(),texture.row_terms.data(),&mapped[batch.size]);
   ++batch.size;
  }
  const auto view=out.view(grid,0);
  for(uint32_t at=0;at<grid.extent.size();++at){
   if((at&4095)==0)checkpoint(cancelled);
   source.first=at;
   for(unsigned j=0;j<batch.size;++j)
    mapped[j]=source_coordinates(batch.jobs[j],at%grid.extent.width,at/grid.extent.width);
   write_output(view,grid,at,at,in.parameters,in.layers.data(),in.tiles.data(),pixels,padding.data(),bake,prepared,source,q);
  }
 }
 checkpoint(cancelled);return out;
}
}

namespace dome::paint_grid {
struct Filter::Impl { Tables tables; };
Filter::Filter(const Tables &tables):impl_(std::make_unique<Impl>(Impl{tables})){}
Filter::~Filter()=default;
void Filter::run(const Batch &batch,const Layout &layout,std::span<const Pixel> input,
                 std::span<Pixel> output,const std::function<bool()> &cancelled) {
  validate(batch,layout,input.size(),output.size());
  for(size_t i=0;i<output.size();++i) {
    if((i&255)==0)checkpoint(cancelled);
    output[i]=filter(i,batch,layout,impl_->tables,input.data());
  }
  checkpoint(cancelled);
}
}
