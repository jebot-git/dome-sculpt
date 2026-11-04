// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-paint-sampling.hpp"
namespace {
Scene native_grid(Material::AlphaMode alpha){
 Scene s;s.active().visible=false;auto id=s.add("Native independent UV pixel field",Volume{});s.select(id);auto &o=s.active();o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(quad(),64);o.material.color={.47f,.67f,.35f,.7f};o.material.emissive={.02f,.04f,.03f};o.material.unlit=false;o.material.alpha=alpha;o.material.alpha_cutoff=.45f;o.material.metallic=.2f;o.material.roughness=.7f;
 Brush brush;brush.normal={0,0,1};brush.from=brush.to={-.25f,.1f,0};brush.radius=.3f;brush.strength=.8f;PaintSettings ink;ink.front_only=false;ink.color={.8f,.15f,.3f,.35f};o.painted->begin_stroke();o.painted->stamp(brush,ink);check(o.painted->end_stroke(),"Native pixel fixture has actual partial RGBA paint");
 o.painted->add_layer("Actual physical detail");brush.from=brush.to={.25f,-.1f,0};brush.radius=.28f;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={2.4f,.015f,.8f};o.painted->begin_stroke();o.painted->stamp(brush,ink);check(o.painted->end_stroke(),"Native pixel fixture has nonconstant actual F32 detail");o.painted->layer_properties("Actual physical detail",true,.63f);ink.channel=PaintChannel::Mask;ink.mask=.35f;brush.radius=.12f;o.painted->begin_stroke();o.painted->stamp(brush,ink);check(o.painted->end_stroke(),"Native pixel fixture has independent common coverage");
 o.painted->add_layer("Hidden physical");ink.channel=PaintChannel::Emission;ink.emission={10000,10000,10000};o.painted->begin_stroke();o.painted->stamp(brush,ink);o.painted->end_stroke();o.painted->layer_properties("Hidden physical",false,1);s.validate();return s;
}
Scene native_split(const Scene &before){
 const auto &m=before.active().painted->mesh();Mesh target;std::vector<uint32_t> map;
 for(auto v:m.indices){map.push_back(v);target.positions.push_back(m.positions[v]);target.normals.push_back(m.normals[v]);const auto uv=m.uv[v];target.uv.push_back({.5f-.7f*(uv[1]-.5f),.5f+.7f*(uv[0]-.5f)});target.indices.push_back(uint32_t(target.indices.size()));}
 auto retained=detail::retain_native_paint(before.active(),target,map);auto current=before;current.active().painted=retained.paint;current.active().material_bindings=detail::bind_native_paint(retained.owner,{});current.validate();check(m.positions.size()==4&&target.positions.size()==6,"Pixel fixture retains actual source4/current6 geometry identities");for(size_t c=0;c<map.size();++c){check(length(target.positions[c]-m.positions[map[c]])==0&&length(target.normals[c]-m.normals[map[c]])==0,"Independent per-corner POSITION/N oracle preserves exact source map");}check(!current.active().material_bindings->normal_frames&&!current.active().painted->normal_projection(),"Native grid pixels do not borrow or manufacture normal frames");return current;
}
}
