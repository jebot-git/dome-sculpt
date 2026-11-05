// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "canonical-paint-sampling.hpp"
#include "canonical-paint-prospect.hpp"
#include "baked-normal-field.hpp"
#include <iostream>
#include <numeric>
using namespace dome;
namespace {
void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
void ready(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error.empty()?"Actual canonical quota worker did not settle":d.frame().state->error);}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;auto raw=Scene::load(argv[1]);auto &source=raw.active();
 const auto old=source.painted;require(old&&old->baked()&&!old->has_physical_emission()&&!source.material_bindings,"Quota source is actual native baked canonical data");
 auto mesh=old->mesh();const auto p=mesh.positions.front(),n=mesh.normals.front();const auto uv=mesh.uv.front();
 mesh.positions.resize(15625,p);mesh.normals.resize(15625,n);mesh.uv.resize(15625,uv);
 source.painted=old->reparameterize(mesh);require(source.painted->baked()==old->baked()&&source.painted->layers().size()==old->layers().size(),"Actual identity-UV transfer preserves original baked and editable data");for(size_t i=0;i<old->layers().size();++i){const auto &a=old->layers()[i],&b=source.painted->layers()[i];require(a.tiles==b.tiles&&a.pbr_tiles==b.pbr_tiles&&a.emission_tiles==b.emission_tiles&&a.opacity==b.opacity&&a.visible==b.visible,"Every actual legacy channel and layer metadata survives unused-vertex extension exactly");}
 auto model=DeformationState(mesh);model.morphs=MorphSet(mesh);
 for(size_t slot=0;slot<128;++slot){MorphTarget t;t.name="actual-zero-"+std::to_string(slot);t.delta.resize(mesh.positions.size());model.morphs->targets.push_back(std::move(t));}
 model.validate(mesh);raw.set_deformation(source.id,std::move(model));raw.validate();
 Document doc;doc.replace(raw);ready(doc);doc.convert_baked_normals();ready(doc);
 doc.edit_scene("Real physical quota stroke",[](Scene &s,const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Quota stroke cancelled");auto &o=s.active();o.painted=o.painted->snapshot();PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={12,.002f,3};Brush brush;brush.normal={0,0,1};const auto &m=o.painted->mesh();brush.from=brush.to=(m.positions[m.indices[0]]+m.positions[m.indices[1]]+m.positions[m.indices[2]])/3;brush.radius=.2f;brush.strength=.73f;o.painted->begin_stroke();o.painted->stamp(brush,ink);if(!o.painted->end_stroke())throw std::runtime_error("Quota physical stroke was empty");});ready(doc);
 const auto scene=doc.copy_scene();const auto &o=scene.active();require(o.deformation->resource_entries()==2'000'000&&o.material_bindings->canonical_attributes&&o.painted->baked()->normal_field&&o.painted->has_physical_emission(),"Actual public Convert and physical brush retain genuine two-million-entry model and field");
 auto current=o.painted->mesh();std::vector<uint32_t> map(current.positions.size());std::iota(map.begin(),map.end(),0);for(auto &q:current.uv)q={.1f+.8f*q[0],.1f+.8f*q[1]};
 auto usage=detail::preflight_canonical_paint_models(scene,o.id,2'000'000);require(usage.deformation_entries==8'000'000&&usage.deformation_slots==4,"Actual current/UV-origin/canonical-origin/new-before slots hit exact unchanged eight-million cap");
 auto accepted=detail::CanonicalPaintSampling::create(scene,o.id,current,map);require(accepted.deformation==o.deformation&&accepted.owner->before()->deformation==o.deformation&&accepted.owner->normal_uv()->origin()==o.material_bindings->canonical_attributes,"Exact-cap identity model shares genuine immutable current model while retaining distinct inline before slot");
 const auto old_vertex=current.indices[0],fresh=uint32_t(current.positions.size());current.positions.push_back(current.positions[old_vertex]);current.normals.push_back(current.normals[old_vertex]);current.uv.push_back(current.uv[old_vertex]);map.push_back(old_vertex);current.indices[0]=fresh;
 const auto future=detail::canonical_paint_model_entries(o.deformation.get(),o.painted->mesh(),current,map);require(future==2'000'128,"One authentic seam split broadcasts all128 real target arrays");
 bool refused=false;try{detail::CanonicalPaintSampling::create(scene,o.id,current,map);}catch(const std::length_error &e){refused=std::string_view(e.what()).find("prospective decoded model quota exceeded before allocation")!=std::string_view::npos;std::cout<<"EXPECTED "<<e.what()<<std::endl;}
 require(refused,"Actual canonical/F32 seam prospective cap must refuse before copying new model arrays");
 require(doc.copy_scene().active().painted->mesh_owner()==o.painted->mesh_owner()&&doc.copy_scene().active().painted->baked()==o.painted->baked()&&doc.copy_scene().active().deformation==o.deformation,"Detached quota rejection preserves actual native current Paint/model owners");
 std::cout<<"PASS actual canonical physical eight-million boundary; slots="<<usage.deformation_slots<<" over="<<future+6'000'000<<std::endl;return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
