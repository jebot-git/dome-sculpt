// SPDX-License-Identifier: GPL-3.0-or-later
#define main initial_native32_reference
#include "native-paint-sampling.cpp"
#undef main
#include "dome/bake-authoring.hpp"
#include "cut-color.hpp"
#include <set>
namespace {
Scene retained_scene(bool physical) {
 auto s=source();
 if(!physical){
  auto &object=s.active();
  object.material.emissive={};
  object.material.color={.08f,.12f,.8f,1};
  object.painted=std::make_shared<PaintedMesh>(quad(),64);
  PaintSettings red;red.color={.95f,.04f,.02f,1};red.front_only=false;
  Brush brush;brush.from=brush.to={-.45f,.29f,0};brush.radius=.22f;brush.strength=1;
  object.painted->begin_stroke();
  object.painted->stamp(brush,red);
  check(object.painted->end_stroke(),"The retained LDR source contains committed asymmetric colour");
 }
 auto mesh=s.active().painted->mesh();
 for(auto &uv:mesh.uv)uv={.5f-.7f*(uv[1]-.5f),.5f+.7f*(uv[0]-.5f)};
 std::vector<uint32_t> map(mesh.positions.size());
 std::iota(map.begin(),map.end(),0u);
 auto retained=detail::retain_native_paint(s.active(),mesh,map);
 s.active().painted=retained.paint;
 s.active().material_bindings=detail::bind_native_paint(retained.owner,{});
 return s;
}

void retained_source_bake(ObjectBakeSettings settings) {
 auto scene=retained_scene(false);
 const auto source_id=scene.active().id;
 check(!detail::cut_color_supported(scene.active()),"Native retained LDR source is not silently eligible for an unadapted plane cut");
 refuses([&]{detail::require_cut_color_supported(scene.active());},"Actual cut readiness refuses the unadapted source domain");
 const auto destination=scene.add("Ordinary bake target",Volume{});
 auto &target=scene.object(destination);
 target.mesh_active=true;
 auto low=quad();
 for(auto &p:low.positions)p.z=-.01f;
 target.painted=std::make_shared<PaintedMesh>(std::move(low),64);
 scene.select(destination);
 const auto baseline=scene.snapshot();
 const auto before=baseline.encode();

 auto reference=baseline.snapshot();
 detail::restore_native_paint(reference,source_id);
 reference.select(destination);
 const auto control=bake_object(reference,source_id,settings);
 const auto report=bake_object(scene,source_id,settings);
 const auto &output=*scene.active().painted->baked();
 const auto &ordinary=*reference.active().painted->baked();
 check(report.projected==control.projected&&report.projected>3500&&output.coverage==ordinary.coverage,
       "Bake from original and independently retained paint covers the same projected texels");
 auto pixel=[](const Image &image,size_t x,size_t y) {
  const auto offset=(y*image.width+x)*4;
  return std::array{image.rgba.at(offset),image.rgba.at(offset+1),image.rgba.at(offset+2)};
 };
 const auto accent=pixel(output.maps.color,14,43),background=pixel(output.maps.color,49,20);
 check(accent[0]>accent[2]+80&&background[2]>background[0]+80,
       "The red stroke stays at its source-space position rather than following the rotated writable UVs");
 size_t maximum=0;
 for(size_t i=0;i<output.maps.color.rgba.size();++i)
   maximum=std::max(maximum,size_t(std::abs(int(output.maps.color.rgba[i])-int(ordinary.maps.color.rgba[i]))));
 std::cout<<"Retained source Bake maximum RGB8 difference "<<maximum<<'\n';
 check(maximum<=2,"Retained paint matches the original independent Bake within two RGB8 levels");
 auto isolated=scene.snapshot();
 isolated.object(destination)=baseline.object(destination);
 check(isolated.encode()==before,"Successful source Bake changes only the target and leaves retained paint exact");

 auto cancelled=baseline.snapshot();
 refuses([&]{bake_object(cancelled,source_id,settings,{},[](float progress){
   if(progress>=1)throw std::runtime_error("Cancel retained source Bake before publication");
 });},"Publication cancellation rejects an already computed source Bake");
 check(cancelled.encode()==before,"Late cancellation keeps the complete retained source and unmodified destination");
}
}
int main()try{
 auto s=retained_scene(true);const auto target=s.active().id;const auto high=s.add("Real nonconstant normals",Volume{});auto &o=s.object(high);o.mesh_active=true;auto highmesh=quad();for(size_t v=0;v<highmesh.positions.size();++v){highmesh.positions[v].z=.02f;highmesh.normals[v]=normalized(Vec3{highmesh.positions[v].x*.4f,highmesh.positions[v].y*.3f,1});}o.painted=std::make_shared<PaintedMesh>(highmesh,64);s.select(target);const auto before=s.encode();ObjectBakeSettings settings;settings.front_mm=settings.back_mm=100;settings.ao_samples=0;settings.padding=0;settings.allow_partial=true;
 refuses([&]{bake_object(s,high,settings);},"Actual unsupported normal Bake refuses before publication");check(s.encode()==before,"Unsupported native target Bake keeps every Scene owner/history byte");
 BakeSource source;source.mesh=highmesh;BakeOptions options;options.resolution=64;options.front_distance=options.back_distance=.1f;options.ao_samples=0;options.padding=0;options.fail_on_miss=false;auto baked=bake_mesh(s.active().painted->mesh(),source,options);std::set<std::array<uint8_t,3>> values;for(size_t p=0;p<baked.normal.rgba.size();p+=4)values.insert({baked.normal.rgba[p],baked.normal.rgba[p+1],baked.normal.rgba[p+2]});check(baked.projected>1000&&values.size()>8,"Genuine bare bake computes a nonconstant normal field for the transition probe");BakedTextures payload;payload.maps=std::move(baked.maps);payload.maps.normal=std::move(baked.normal);for(auto c:baked.coverage)payload.coverage.push_back(c==1||c==2?255:0);auto invalid=s;invalid.active().painted=invalid.active().painted->snapshot();invalid.active().painted->install_bake(std::move(payload));check(!invalid.active().material_bindings->native_paint_sampling->matches_current(*invalid.active().painted),"Current native lease cannot authenticate a newly interpreted bake");refuses([&]{invalid.validate();},"Unbound direct baked transition refuses Scene admission");refuses([&]{invalid.encode();},"Unbound direct baked transition refuses writer admission");check(s.encode()==before,"Failed direct transition cannot mutate original source/paint");
 retained_source_bake(settings);
 std::cout<<"PASS "<<checks<<" native32 transition controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
