// SPDX-License-Identifier: GPL-3.0-or-later
#define main established_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/source-paint.hpp"
#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include "dome/polygon-scene.hpp"
#include "paint-overlay-alpha.hpp"
#include "polygon-conversion.hpp"
namespace {
std::shared_ptr<MeshSnapshot> visible(const std::shared_ptr<PaintedMesh> &paint,const Material &m) {
 auto out=std::make_shared<MeshSnapshot>();out->mesh=paint->mesh();out->ranges.push_back({1,0,out->mesh.indices.size(),m,paint->preview(m),{},0});return out;
}
void ink(PaintedMesh &paint,PaintSettings setting){Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=2;b.strength=.8f;setting.front_only=false;paint.begin_stroke();paint.stamp(b,setting);check(paint.end_stroke(),"Real native source-grid stroke changes sparse paint");}
}
int main(int argc,char **argv)try {
 NativeGraphics graphics;std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';
 check(argc<2||std::string_view(argv[1])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos),"Requested hardware test requires actual A770");
 auto raw=quad();raw.uv={{-1.25f,-.5f},{1.75f,-.5f},{1.75f,2.5f},{-1.25f,2.5f}};const auto target=quad();
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image({128,192,64,128});maps->metal_rough.image=image({255,180,140,255});maps->emission.image=image({100,80,50,255});
 PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;auto before=PaintedMesh::from_maps(raw,maps,64,policy);const auto original=before->encode();
 const std::array<uint32_t,4> vertices{0,1,2,3};auto paint=prepare_source_paint(before,target,vertices);auto unpainted=paint->snapshot();
 auto reference=PaintedMesh::from_maps(target,maps,64);PaintSettings setting;setting.color={.2f,.7f,.3f,.15f};
 ink(*paint,setting);ink(*reference,setting);setting.channel=PaintChannel::Metallic;setting.metallic=.8f;ink(*paint,setting);ink(*reference,setting);
 setting.channel=PaintChannel::Roughness;setting.roughness=.35f;ink(*paint,setting);ink(*reference,setting);
 setting.channel=PaintChannel::Emission;setting.emission={.07f,.11f,.03f};ink(*paint,setting);ink(*reference,setting);
 paint->layer_properties("Fractional paint",true,.63f);reference->layer_properties("Fractional paint",true,.63f);
 Material material=white();material.color={.7f,.8f,.9f,.8f};material.metallic=.3f;material.roughness=.7f;material.emissive={.1f,.2f,.05f};
 auto marker=paint->preview(material);check(marker->source_only&&marker->overlay&&marker->color.rgba.empty()&&marker->metal_rough.rgba.empty()&&marker->emission.rgba.empty(),"Painted source preview publishes sparse GPU input without dense CPU composite images");
 check(marker==paint->preview(material)&&marker->overlay->source_maps()==paint->source_maps(),"Repeated preview retains exact packet and source-map identity");
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;renderer.set_output_encoding(SurfaceOutputEncoding::Linear);int maximum=0;
 for(bool unlit:{true,false})for(auto mode:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
  material.unlit=unlit;material.alpha=mode;material.alpha_cutoff=.25f;auto actual=visible(paint,material);auto paired=std::make_shared<MeshSnapshot>();paired->mesh=target;
  paired->ranges.push_back({1,0,target.indices.size(),material,reference->composite(material),{},0});
  const auto wanted=draw(renderer,paired),got=draw(renderer,actual);const int error=delta(got,wanted);maximum=std::max(maximum,error);std::cout<<"PAIRED overlay unlit="<<unlit<<" alpha="<<int(mode)<<" max="<<error<<'\n';
  check(error<=2,"Actual SurfaceRenderer sparse source overlay agrees with established constant-source composite within two codes");
  MaterialSurfaceQuery query(actual->mesh,actual->ranges),control(paired->mesh,paired->ranges);
  for(auto xy:{std::array<float,2>{0,0},std::array<float,2>{.5f,.4f}})
   check(bool(query.raycast({xy[0],xy[1],1},{0,0,-1},2))==bool(control.raycast({xy[0],xy[1],1},{0,0,-1},2)),"Actual alpha picker agrees with established material visibility");
 }
 auto state=paint->encode();auto restored=PaintedMesh::decode(state,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true});
 check(draw(renderer,visible(restored,material))==draw(renderer,visible(paint,material)),"Source ownership restart preserves the complete actual rendered frame");
 auto forged=std::make_shared<TextureMaps>(*marker);forged->source_maps=std::make_shared<const PaintSourceMaps>(*paint->source_maps());auto invalid=visible(paint,material);invalid->ranges[0].textures=forged;
 check(rejects([&]{MaterialSurfaceQuery bad(invalid->mesh,invalid->ranges);})&&rejects([&]{draw(renderer,invalid);}),"Equal-valued foreign source-map owners cannot borrow a live overlay packet");
 auto hidden=paint->snapshot();hidden->layer_properties("Hidden actual paint",false,.63f);
 check(!hidden->preview(material)->overlay&&draw(renderer,visible(hidden,material))==draw(renderer,visible(unpainted,material)),"Hiding actual paint restores exact original source rendering");
 SurfaceTextureLimits limited;limited.max_bytes=100'000;SurfaceRenderer bounded(limited);
 check(rejects([&]{draw(bounded,visible(paint,material));})&&bounded.texture_stats().images==0&&paint->encode()==state,"Shared source-plus-overlay GPU quota refuses before publication and preserves all paint");
 // Compound ownership retains separate explicit capabilities, including a
 // physical value smaller than the legacy UNORM8 emission precision.
 paint->add_layer("Physical emission");setting.channel=PaintChannel::Emission;setting.emission_units=PaintEmissionUnits::Physical;setting.emission={100,.005f,12};ink(*paint,setting);
 const auto hdr=paint->encode();check(hdr.at("version")==4&&rejects([&]{PaintedMesh::decode(hdr,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true});})&&
   rejects([&]{PaintedMesh::decode(hdr,nullptr,false,nullptr,nullptr,true);}),"Neither single capability can decode combined source/physical paint");
 auto hdr_read=PaintedMesh::decode(hdr,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true,true});check(hdr_read->encode()==hdr,"Both explicit capabilities reconstruct exact source/physical paint");
 Scene scene;scene.active().painted=paint;scene.active().mesh_active=true;scene.active().material=material;const auto saved=scene.encode();
 check(saved.at("version")==28&&Scene::decode(saved).encode()==saved,"Actual owning Scene28 restores compound physical source paint");
 auto capsule=capture_polygon_origin(scene,scene.active().id);auto retained=scene.snapshot();retained.active().painted=unpainted;retained.active().shape_source=retained.active().rig_source=capsule;
 const auto retained_bytes=retained.encode();check(Scene::decode(retained_bytes).encode()==retained_bytes,"Retained shape/rig owners preserve compound source/physical paint");
 check(before->encode()==original,"Original unmodified source owner remains exact after GPU, picking and persistence");
 std::cout<<"PASS "<<checks<<" source paint overlay checks max_pair_error="<<maximum<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
