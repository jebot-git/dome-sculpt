// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include <barrier>
#include <chrono>
#include <future>
#include <iostream>
using namespace dome;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);std::cout<<"PASS "<<m<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh quad(){Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
std::shared_ptr<const PaintSourceMaps> source(){auto m=std::make_shared<PaintSourceMaps>();m->color.image=std::make_shared<const Image>(Image{2,1,{128,64,255,0,255,128,64,255}});return m;}
Brush brush(){Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=.4f;return b;}
void paint(PaintedMesh&p,std::array<float,4>color){PaintSettings s;s.color=color;p.begin_stroke();p.stamp(brush(),s);p.end_stroke();}
Material white(){Material m;m.color={1,1,1,1};return m;}
}
int main(int argc,char**argv)try{
  constexpr size_t mib=1024*1024;check(paint_composite_cache_stats().limit_bytes==512*mib,"Default composite retention fits the measured two-baseline source scene within an explicit512MiB cap");set_paint_composite_cache_budget(0);set_paint_composite_cache_budget(256*mib);
  auto p=PaintedMesh::from_maps(quad(),source(),64);auto material=white();auto a=p->composite(material);auto stats=paint_composite_cache_stats();
  check(p->composite(material)==a&&paint_composite_cache_stats().builds==stats.builds,"Repeated unchanged compositing reuses one immutable renderer-ready map");
  auto snapshot=p->snapshot();check(snapshot->composite(material)==a,"Detached geometry workers share unchanged immutable composite ownership");
  auto renamed=material;renamed.name="Another label";renamed.toon=true;renamed.unlit=true;renamed.alpha=Material::AlphaMode::Mask;renamed.alpha_cutoff=.3f;renamed.double_sided=true;
  check(p->composite(renamed)==a,"Nonpixel material names and shader render state do not force CPU texture recompositing");
  auto uv=quad();uv.uv_pins.assign(4,1);check(p->reparameterize(uv)->composite(material)==a,"Unchanged source UV geometry/pin edits share the same immutable texture maps");
  auto tint=material;tint.color[0]=.5f;auto tinted=p->composite(tint);check(tinted!=a&&tinted->color.rgba!=a->color.rgba&&p->composite(material)==a,"Material factor edits have distinct exact cache keys and bounded prior factors remain reusable");
  auto emission=material;emission.emissive={1,.5f,.25f};auto emitted=p->composite(emission);check(emitted->emission.rgba!=a->emission.rgba,"Emission factors participate in composite cache content identity");
  auto scalar=material;scalar.metallic=1;scalar.roughness=.2f;auto orm=p->composite(scalar);check(orm->metal_rough.rgba!=a->metal_rough.rgba,"Linear metallic and roughness factors participate in cache identity");
  auto invalid=material;invalid.color[0]=NAN;check(rejects([&]{p->composite(invalid);}),"Invalid material input still fails before a cached frame can be returned");
  auto left=p->snapshot(),right=p->snapshot();paint(*left,{1,0,0,1});paint(*right,{0,0,1,1});auto red=left->composite(material),blue=right->composite(material);
  check(left->revision()==right->revision()&&red->color.rgba!=blue->color.rgba,"Independently mutated snapshots at equal numeric revisions cannot collide in the composite cache");
  check(left->composite(material)==red&&right->composite(material)==blue&&snapshot->composite(material)==a,"Bounded content lookup preserves forked source/paint frames and original snapshot immutability");
  auto before=left->composite(material);left->begin_stroke();PaintSettings settings;settings.color={0,1,0,1};left->stamp(brush(),settings);auto temporary=left->composite(material);left->cancel_stroke();
  check(temporary!=before&&left->composite(material)==before&&temporary->color.rgba!=before->color.rgba,"Cancelling a preview restores its original content key without changing renderer-held maps");
  check(left->undo()&&left->composite(material)->color.rgba==a->color.rgba&&left->redo()&&left->composite(material)->color.rgba==red->color.rgba,"Undo and redo invalidate stale previews while retaining exact original channel output");
  left->layer_properties("Renamed",true,1);auto unchanged=left->composite(material);check(left->composite(material)==unchanged,"Layer label edits preserve stable composite content");
  left->layer_properties("Hidden",false,1);auto hidden=left->composite(material);check(rejects([&]{paint(*left,{0,1,0,1});})&&left->composite(material)==hidden&&hidden->color.rgba==a->color.rgba,"Rejected hidden-layer painting preserves unchanged visible source composites");
  left->layer_properties("Hidden opacity",false,.5f);check(left->composite(material)==hidden,"Hidden opacity changes reuse the unchanged visible composite");
  left->layer_properties("Visible",true,1);check(left->composite(material)->color.rgba!=hidden->color.rgba,"Revealing changed hidden paint creates a fresh visible result");
  left->layer_properties("Zero",true,0);auto zero=left->composite(material);paint(*left,{1,1,0,1});check(left->composite(material)==zero,"Zero-opacity layer edits reuse unchanged visible texture maps");
  left->add_layer("Empty");auto empty=left->composite(material);left->move_layer(-1);check(left->composite(material)==empty,"Empty-layer addition and ordering do not invalidate visible pixels");left->delete_layer();check(left->composite(material)==empty,"Removing an empty layer preserves visible source maps");
  auto stack=PaintedMesh::from_maps(quad(),source(),64);paint(*stack,{1,0,0,1});auto bottom=stack->composite(material);stack->add_layer("Blue");paint(*stack,{0,0,1,1});auto top=stack->composite(material);stack->move_layer(-1);
  check(stack->composite(material)->color.rgba!=top->color.rgba,"Reordering two visible painted layers invalidates the composite key");stack->delete_layer();
  check(stack->composite(material)->color.rgba==bottom->color.rgba,"Deleting a visible painted layer restores exact lower-layer output without a stale cached frame");
  PaintedMesh transferred(quad(),64);auto off=brush();off.from=off.to={-.5f,0,0};PaintSettings red_settings;red_settings.color={1,0,0,1};transferred.begin_stroke();transferred.stamp(off,red_settings);transferred.end_stroke();auto old_uv=transferred.composite(material);auto mirrored=quad();for(auto &uv:mirrored.uv)uv[0]=1-uv[0];auto new_uv=transferred.reparameterize(mirrored)->composite(material);
  check(old_uv!=new_uv&&old_uv->color.rgba!=new_uv->color.rgba,"Paint-only UV transfer creates a new pixel identity while prior worker maps remain immutable");
  auto cold=PaintedMesh::from_maps(quad(),source(),64);std::barrier gate(8);std::vector<std::future<std::shared_ptr<const TextureMaps>>> jobs;auto initial=paint_composite_cache_stats();
  for(int n=0;n<8;++n){auto copy=cold->snapshot();jobs.push_back(std::async(std::launch::async,[copy,material,&gate]{gate.arrive_and_wait();return copy->composite(material);}));}
  std::vector<std::shared_ptr<const TextureMaps>> results;for(auto &job:jobs)results.push_back(job.get());bool same=true;for(auto &result:results)same&=result==results[0];
  check(same&&paint_composite_cache_stats().builds==initial.builds+1,"Concurrent detached snapshot readers serialize one cold composite build and share its immutable result");
  set_paint_composite_cache_budget(0);auto shared=source();auto one=PaintedMesh::from_maps(quad(),shared,64),two=PaintedMesh::from_maps(quad(),shared,64);auto one_map=one->composite(material),two_map=two->composite(material);
  size_t derived=one_map->color.rgba.size()+one_map->metal_rough.rgba.size()+one_map->emission.rgba.size()+one_map->color_coverage.rgba.size()+one_map->emission_coverage.rgba.size();set_paint_composite_cache_budget(1024*1024);one->composite(material);two->composite(material);auto accounted=paint_composite_cache_stats();
  check(accounted.retained_maps==2&&accounted.retained_bytes==derived*2+shared->color.image->rgba.size(),"Global cache charges original shared source image storage once across retained material lineages");
  set_paint_composite_cache_budget(derived+shared->color.image->rgba.size());check(paint_composite_cache_stats().retained_maps==1&&paint_composite_cache_stats().retained_bytes<=paint_composite_cache_stats().limit_bytes,"Byte-budget reduction evicts least-recently-used strong retention without exceeding the quota");
  std::weak_ptr<const TextureMaps> old=one_map;one_map.reset();check(old.expired(),"Eviction releases discarded cached frames instead of retaining prior history indefinitely");
  check(two->composite(material)==two_map,"An immutable renderer-held result stays reusable after unrelated cache eviction");
  auto oversized=std::make_shared<PaintSourceMaps>();oversized->color.image=std::make_shared<const Image>(Image{128,128,std::vector<uint8_t>(128*128*4)});
  auto big=PaintedMesh::from_maps(quad(),oversized,64);auto retained_before=paint_composite_cache_stats();big->composite(material);
  check(paint_composite_cache_stats().retained_maps==retained_before.retained_maps&&paint_composite_cache_stats().retained_bytes==retained_before.retained_bytes,
    "An individually uncacheable map does not evict unrelated useful retained frames");
  set_paint_composite_cache_budget(0);check(paint_composite_cache_stats().retained_maps==0&&paint_composite_cache_stats().retained_bytes==0,"Zero budget disables all global strong composite retention");
  check(two->composite(material)==two_map&&paint_composite_cache_stats().retained_maps==0,"Zero-retention mode still shares live immutable renderer-held weak results");
  check(rejects([&]{set_paint_composite_cache_budget(513*mib);}),"Composite retention settings cannot exceed the native hard cache budget");
  set_paint_composite_cache_budget(0);set_paint_composite_cache_budget(512*mib);
  for(int n=0;n<131;++n){auto variant=material;variant.color[0]=float(n)/130;p->composite(variant);}
  check(paint_composite_cache_stats().retained_maps==128,"Independent material variants cannot exceed the hard128-entry retention limit");
  set_paint_composite_cache_budget(0);set_paint_composite_cache_budget(256*mib);
  set_paint_composite_cache_budget(0);set_paint_composite_cache_budget(512*mib);
  auto baked_paint=PaintedMesh::from_maps(quad(),shared,64);BakedTextures base;
  base.maps.color={64,64,std::vector<uint8_t>(64*64*4)};base.maps.metal_rough=base.maps.emission=base.maps.normal=base.maps.color;
  for(size_t at=0;at<64*64;++at)for(int k=0;k<4;++k){base.maps.color.rgba[at*4+k]=std::array<uint8_t,4>{30,60,220,255}[k];base.maps.metal_rough.rgba[at*4+k]=std::array<uint8_t,4>{255,180,0,255}[k];base.maps.emission.rgba[at*4+k]=k==3?255:0;base.maps.normal.rgba[at*4+k]=std::array<uint8_t,4>{128,204,230,255}[k];}
  base.coverage.assign(64*64,128);baked_paint->install_bake(base);auto baked_a=baked_paint->composite(material);auto variant=material;variant.color[0]=.5f;auto baked_b=baked_paint->composite(variant);
  auto derived_bytes=[](const TextureMaps&m){size_t n=0;for(auto image:{&m.color,&m.metal_rough,&m.emission,&m.normal,&m.color_coverage,&m.emission_coverage,&m.baked_normal,&m.baked_coverage})n+=image->rgba.size();return n;};auto alias_bytes=base.storage_bytes()+shared->color.image->rgba.size();
  check(baked_a->baked_base==baked_paint->baked()&&baked_b->baked_base==baked_a->baked_base&&baked_a->baked_coverage.rgba[0]==128&&baked_a->color_coverage.rgba[0]==128,"Canonical raw normal/coverage and baked color replacement remain independently available to typed materials");
  check(paint_composite_cache_stats().retained_maps==2&&paint_composite_cache_stats().retained_bytes==derived_bytes(*baked_a)+derived_bytes(*baked_b)+alias_bytes,"Cache accounts every aliased baked image and byte coverage once across material variants plus all derived GPU payloads");
  set_paint_composite_cache_budget(derived_bytes(*baked_b)+alias_bytes);check(paint_composite_cache_stats().retained_maps==1&&paint_composite_cache_stats().retained_bytes==derived_bytes(*baked_b)+alias_bytes,"Eviction releases one alias reference without losing or double-charging shared canonical source data");
  auto invalid_bake=base;invalid_bake.maps.baked_base=baked_paint->baked();check(rejects([&]{baked_paint->install_bake(invalid_bake);})&&baked_paint->baked()==baked_a->baked_base,"Baked payload validation rejects nested retained aliases without replacing the valid immutable base");
  invalid_bake=base;invalid_bake.maps.emission_coverage=baked_a->emission_coverage;
  check(rejects([&]{baked_paint->install_bake(invalid_bake);})&&baked_paint->baked()==baked_a->baked_base,
        "Baked payloads cannot smuggle derived emission coverage or bypass its separate cache accounting");
  baked_paint->clear_bake();check(baked_paint->preview(material)->source_only&&baked_a->baked_base&&baked_a->baked_normal.rgba[1]==204,"Clearing a bake restores raw eligibility while renderer-held canonical snapshots remain immutable");
  set_paint_composite_cache_budget(0);check(paint_composite_cache_stats().retained_maps==0&&paint_composite_cache_stats().retained_bytes==0,"Zero retention releases complete aliased baked accounting even while readers own old snapshots");set_paint_composite_cache_budget(256*mib);
  struct Profile {std::vector<std::shared_ptr<PaintedMesh>> paint;std::vector<Material> material;};std::vector<Profile> profiles;
  for(int arg=1;arg<argc;++arg){auto path=std::filesystem::path(argv[arg]);auto id=path.filename().string().find("female")!=std::string::npos?"vroid-beta-female":"vroid-beta-male";auto creator=create_humanoid(humanoid_baseline_defaults(id),{},path.parent_path().parent_path());Profile profile;
    for(auto &part:creator.parts){PaintSurfaceOptions options;options.policy=PaintUvPolicy::SourceAuthored;profile.paint.push_back(PaintedMesh::from_maps(part.mesh,part.source_maps,256,options));auto m=white();m.color=part.color;m.roughness=part.roughness;profile.material.push_back(m);}profiles.push_back(std::move(profile));
  }
  for(size_t model=0;model<profiles.size();++model){auto &profile=profiles[model];std::vector<std::shared_ptr<const TextureMaps>> maps;auto start=std::chrono::steady_clock::now();for(size_t i=0;i<profile.paint.size();++i)maps.push_back(profile.paint[i]->composite(profile.material[i]));auto first=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();start=std::chrono::steady_clock::now();bool reused=true;
    for(size_t i=0;i<profile.paint.size();++i)reused&=profile.paint[i]->snapshot()->composite(profile.material[i])==maps[i];
    auto repeated=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    check(reused&&profile.paint.size()==19,"Actual19-part source geometry snapshots reuse every unchanged renderer-ready map without recompositing pixels");std::cout<<"PROFILE model="<<model<<" cold_ms="<<first<<" unchanged_ms="<<repeated<<" retained_bytes="<<paint_composite_cache_stats().retained_bytes<<'\n';
  }
  if(profiles.size()>1)for(size_t budget:{size_t(256*mib),size_t(512*mib)}){set_paint_composite_cache_budget(0);set_paint_composite_cache_budget(budget);for(int pass=0;pass<2;++pass){auto before=paint_composite_cache_stats();auto start=std::chrono::steady_clock::now();for(auto &profile:profiles)for(size_t i=0;i<profile.paint.size();++i)profile.paint[i]->composite(profile.material[i]);auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();auto after=paint_composite_cache_stats();check(after.retained_bytes<=budget&&after.retained_maps<=128,"Mixed actual source-scene profile respects explicit cache memory and entry bounds");std::cout<<"MIXED budget="<<budget<<" pass="<<pass<<" ms="<<ms<<" builds="<<after.builds-before.builds<<" retained_bytes="<<after.retained_bytes<<'\n';}}
  set_paint_composite_cache_budget(512*mib);std::cout<<"Native composite cache tests complete\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
