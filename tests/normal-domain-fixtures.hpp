// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "prepared-normal-shapes-fixtures.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/normal-sampling.hpp"
#include <numeric>
namespace dome::normal_domain_fixture {
inline Scene model(){
 auto source=prepared_shape_fixture::model(true);auto partial=source.active().painted->snapshot();auto baked=*partial->baked();partial->clear_bake();
 for(size_t at=0;at<baked.coverage.size();++at)baked.coverage[at]=std::array<uint8_t,4>{0,64,128,255}[at%4];
 partial->install_bake(std::move(baked));source.active().painted=partial;source.validate();
 SourceUvBakeInput input;input.paint=partial;input.current_material=source.active().material_bindings;input.current_deformation=source.active().deformation;input.target=partial->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
 for(auto &uv:input.target.uv)for(auto &q:uv)q=.5f+(q-.5f)*.7f;
 SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;auto adopted=adopt_source_uv_bake(bake_source_uv(input,options));
 source.active().painted=adopted.paint;source.active().material_bindings=adopted.material_bindings;source.active().deformation=adopted.deformation;source.active().appearance=adopted.appearance;source.validate();return source;
}
}
