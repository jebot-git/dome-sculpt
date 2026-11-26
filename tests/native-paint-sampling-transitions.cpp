// SPDX-License-Identifier: GPL-3.0-or-later
#define main initial_native32_reference
#include "native-paint-sampling.cpp"
#undef main
#include "dome/bake-authoring.hpp"
#include "cut-color.hpp"
#include <set>
namespace {
Scene retained_scene(bool physical){auto s=source();if(!physical){s.active().material.emissive={};s.active().painted=std::make_shared<PaintedMesh>(quad(),64);}auto mesh=s.active().painted->mesh();for(auto &uv:mesh.uv)uv={.5f-.7f*(uv[1]-.5f),.5f+.7f*(uv[0]-.5f)};std::vector<uint32_t> map(mesh.positions.size());std::iota(map.begin(),map.end(),0u);auto r=detail::retain_native_paint(s.active(),mesh,map);s.active().painted=r.paint;s.active().material_bindings=detail::bind_native_paint(r.owner,{});return s;}
}
int main()try{
 auto s=retained_scene(true);const auto target=s.active().id;const auto high=s.add("Real nonconstant normals",Volume{});auto &o=s.object(high);o.mesh_active=true;auto highmesh=quad();for(size_t v=0;v<highmesh.positions.size();++v){highmesh.positions[v].z=.02f;highmesh.normals[v]=normalized(Vec3{highmesh.positions[v].x*.4f,highmesh.positions[v].y*.3f,1});}o.painted=std::make_shared<PaintedMesh>(highmesh,64);s.select(target);const auto before=s.encode();ObjectBakeSettings settings;settings.front_mm=settings.back_mm=100;settings.ao_samples=0;settings.padding=0;settings.allow_partial=true;
 auto accepted=s;
 const auto result=bake_object(accepted,high,settings);
 check(result.projected>1000&&accepted.active().painted->baked()->normal_interpretation==BakedNormalInterpretation::CanonicalMikk&&
       accepted.active().material_bindings==s.active().material_bindings,"Actual retained target Bake uses its writable raster and canonical normals");
 check(s.encode()==before,"A detached retained target Bake keeps the source scene exact");

 BakeSource source;source.mesh=highmesh;BakeOptions options;options.resolution=64;options.front_distance=options.back_distance=.1f;options.ao_samples=0;options.padding=0;options.fail_on_miss=false;auto baked=bake_mesh(s.active().painted->mesh(),source,options);std::set<std::array<uint8_t,3>> values;for(size_t p=0;p<baked.normal.rgba.size();p+=4)values.insert({baked.normal.rgba[p],baked.normal.rgba[p+1],baked.normal.rgba[p+2]});check(baked.projected>1000&&values.size()>8,"Genuine bare bake computes a nonconstant normal field for the transition probe");
 BakedTextures payload;payload.maps=std::move(baked.maps);payload.maps.normal=std::move(baked.normal);
 payload.normal_interpretation=BakedNormalInterpretation::NativeAttributes;
 for(auto c:baked.coverage)payload.coverage.push_back(c==1||c==2?255:0);
 auto invalid=s;invalid.active().painted=invalid.active().painted->snapshot();invalid.active().painted->install_bake(std::move(payload));
 refuses([&]{invalid.validate();},"Authored normal Bake without its actual frame rejects Scene admission");
 refuses([&]{invalid.encode();},"Authored normal Bake without its actual frame rejects writer admission");
 check(s.encode()==before,"Failed direct transition preserves original source and paint");

 auto plain=retained_scene(false);const auto source_id=plain.active().id;
 check(!detail::cut_color_supported(plain.active()),"Native retained LDR source requires its own plane cut adapter");
 refuses([&]{detail::require_cut_color_supported(plain.active());},"Cut readiness refuses the unsupported source domain");
 auto dest=plain.add("Ordinary bake target",Volume{});auto &target_object=plain.object(dest);target_object.mesh_active=true;auto low=quad();for(auto &p:low.positions)p.z=-.01f;target_object.painted=std::make_shared<PaintedMesh>(low,64);plain.select(dest);
 const auto donor=plain.object(source_id).painted;
 check(bake_object(plain,source_id,settings).projected>1000&&plain.active().painted->baked(),"The retained native donor supplies its paint to the ordinary Bake target");
 check(plain.object(source_id).painted==donor,"Source Bake retains the exact donor paint owner");
 std::cout<<"PASS "<<checks<<" native32 transition controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
