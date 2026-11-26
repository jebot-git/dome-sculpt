// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Shared test-only construction and independent decoder. Include after the
// existing source-prepared-normal-bake.cpp helpers, never recursively include
// that TU from an existing producer. No product resolver/basis/sampler is used
// by decode; callers own their own requirement counter.
namespace advanced_fixture {
template<class Require>
Scene prepared(bool legacy,Require require){
 Scene s;auto &low=s.active();Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};
 low.mesh_active=true;
 // Begin with an ordinary genuine first-Bake target. Independent current
 // material maps will be produced by real SourceUV adoption after authoring.
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,2,{255,60,90,255,50,220,80,255,30,70,240,255,210,180,30,255}});maps->color.texcoord=0;low.painted=capture_source_sampling(m,maps,64);low.material_bindings=blank_typed(legacy);
 DeformationState deformation(m);deformation.morphs=MorphSet(m);auto key=m.positions;key[0].z=.045f;key[2].z=-.025f;deformation.morphs->set_target(m,"Retained native morph",key);deformation.skeleton=Skeleton{{{"root",-1,{},1},{"child",0,{},1}}};deformation.skin=SkinWeights{};deformation.skin->vertices={{{0,.7f},{1,.3f}},{{0,.2f},{1,.8f}},{{0,.4f},{1,.6f}},{{0,.9f},{1,.1f}}};s.set_deformation(1,deformation);
 auto artist=fixture(legacy,false);const auto id=s.add("Immutable artist high",Volume{});auto high=artist.scene.object(artist.source);high.id=id;high.transform.position.z=.15f;high.visible=false;high.locked=true;s.object(id)=std::move(high);s.select(1);s.validate();
 ObjectBakeSettings settings;settings.normal_filter_quality=0; // Preserve the genuine pre-policy first-Bake recipe and source fields.
 settings.prepare_normals=true;settings.front_mm=settings.back_mm=500;settings.ao_samples=0;settings.padding=0;settings.normal_error=.03f;const auto original_high=s.object(id).painted;
 require(bake_object(s,id,settings).normal_prepared&&s.object(id).painted==original_high,"Actual artist first Bake creates source normals while retaining original high source");
 auto p=s.active().painted->snapshot();auto baked=*p->baked();p->clear_bake();for(size_t at=0;at<baked.coverage.size();++at)baked.coverage[at]=std::array<uint8_t,4>{0,64,128,255}[at%4];p->install_bake(std::move(baked));
 p->add_layer("Authored source color");PaintSettings color;color.channel=PaintChannel::Color;color.color={.85f,.12f,.45f,1};color.front_only=false;Brush brush;brush.from=brush.to={-.22f,.17f,0};brush.normal={0,0,1};brush.radius=.18f;p->begin_stroke();p->stamp(brush,color);p->end_stroke();s.active().painted=p;s.validate();return s;
}
inline SourceUvAdoption uv_adoption(const SculptObject &o,SourceNormalUvMethod method,bool seam,size_t replay_cap=256*1024*1024){
 SourceUvBakeInput in;in.paint=o.painted;in.appearance=o.appearance;in.current_material=o.material_bindings;in.current_deformation=o.deformation;in.target=o.painted->mesh();in.source_vertices.resize(in.target.positions.size());std::iota(in.source_vertices.begin(),in.source_vertices.end(),0);
 if(seam){Mesh split;for(size_t c=0;c<in.target.indices.size();++c){const auto v=in.target.indices[c];split.positions.push_back(in.target.positions[v]);split.normals.push_back(in.target.normals[v]);split.uv.push_back(in.target.uv[v]);split.indices.push_back(uint32_t(c));}in.source_vertices=in.target.indices;in.target=std::move(split);}
 for(auto&q:in.target.uv){const auto x=q[0];q={.2f+.6f*q[1],.8f-.6f*x};}
 SourceUvBakeOptions options;options.normal_sampling=method;options.max_working_bytes=replay_cap;options.max_normal_sample_error=.03;return adopt_source_uv_bake(bake_source_uv(in,options));
}
inline Scene uv_source(const Scene &input,SourceNormalUvMethod method,bool seam,size_t replay_cap=256*1024*1024){
 auto out=input.snapshot();auto adoption=uv_adoption(out.active(),method,seam,replay_cap);auto &a=out.active();a.painted=adoption.paint;a.appearance=adoption.appearance;a.deformation=adoption.deformation;a.material_bindings=adoption.material_bindings;out.validate();return out;
}
inline void pose(Scene &s,bool live){if(!live)return;auto d=*s.active().deformation;d.morph_weights={.35f};d.pose.resize(2);d.pose[0].scale={1.2f,.8f,.7f};d.pose[1].scale={.8f,1.1f,1.3f};s.set_deformation(s.active().id,std::move(d));s.validate();}
// Independent declared-frame decoder. No product frame/basis/resolver/sampler.
inline D decode(const SculptObject&o,size_t tri,std::array<float,3>bary,size_t role,bool wrong_domain=false,bool omit=false){
 const auto &b=*o.material_bindings;const auto &set=*b.normal_frames;const auto &f=role==0?*set.pbr:*set.mtoon;const auto &mesh=o.painted->mesh();D N{},T{},B{};std::array<double,2>uv{};
 const auto projection=o.painted->normal_projection();const auto domain=projection->sampling_domain;const auto raster=b.normal_preparation->uv_reprojection;
 for(size_t k=0;k<3;++k){const size_t corner=tri*3+k;const auto v=mesh.indices[corner];D n=vc(f.normals[corner]),t{f.tangents[corner][0],f.tangents[corner][1],f.tangents[corner][2]};
  if(o.deformation){for(size_t slot=0;slot<set.current_targets.size();++slot){const double w=o.deformation->morph_weights.empty()?0:o.deformation->morph_weights[slot];for(const auto&target:f.targets)if(target.id==set.current_targets[slot]){if(!target.normals.empty())n=add(n,mul(vc(target.normals[corner]),w));if(!target.tangents.empty())t=add(t,mul(vc(target.tangents[corner]),w));}}
   if(!o.deformation->pose.empty()){D nn{},tt{};for(auto influence:o.deformation->skin->vertices[v]){const auto a=o.deformation->pose[0].scale,c=o.deformation->pose[1].scale;D scale{a.x,a.y,a.z};if(influence.joint==1)scale={a.x*c.x,a.y*c.y,a.z*c.z};for(size_t axis=0;axis<3;++axis){nn[axis]+=n[axis]/scale[axis]*influence.weight;tt[axis]+=t[axis]*scale[axis]*influence.weight;}}n=nn;t=tt;}}
  n=unit(n);t=unit(t);N=add(N,mul(n,bary[k]));T=add(T,mul(t,bary[k]));B=add(B,mul(unit(crossd(n,t)),bary[k]*f.tangents[corner][3]));
  auto q=mesh.uv[v];if(domain&&!wrong_domain){const auto &coords=role&&!domain->mtoon_coordinates.empty()?domain->mtoon_coordinates:domain->coordinates;q=coords[domain->indices[corner]];}else if(!domain&&wrong_domain&&raster){const auto &before=raster->before_paint->mesh();q=before.uv[before.indices[corner]];}for(size_t axis=0;axis<2;++axis)uv[axis]+=q[axis]*bary[k];
 }
 if(role==0){N=unit(N);T=unit(T);B=unit(B);}else if(b.material.version()==MToonVersion::V1){N=unit(N);T=unit(T);B=mul(unit(crossd(N,T)),f.tangents[tri*3][3]);}
 if(omit)return unit(N);
 // These are the declared live renderer planes, distinct from Bake's normal
 // capture. v2's image remains in its original domain; no atlas rebake here.
 const auto shown=o.painted->preview(o.material);const Image *image=role==0?&shown->normal:&shown->mtoon_normal;
 if(domain){const bool detail=o.painted->baked()&&!o.painted->baked()->maps.normal.rgba.empty();image=(role==0?(detail?domain->resolved_pbr:projection->pbr):(detail?domain->resolved_mtoon:projection->mtoon)).get();}
 auto encoded=sample(*image,uv);const double scale=role==0?1:b.material.parameters.normal_scale;D ts=unit(D{(encoded[0]*2-1)*scale,(encoded[1]*2-1)*scale,encoded[2]*2-1});return unit(add(add(mul(T,ts[0]),mul(B,ts[1])),mul(N,ts[2])));
}
} // namespace advanced_fixture
