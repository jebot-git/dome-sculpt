// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_POST_UV_AVATAR_RENDER_NO_MAIN
#include "post-uv-avatar-render.cpp"
#undef DOME_POST_UV_AVATAR_RENDER_NO_MAIN
#include "dome/source-uv-bake.hpp"
#include "dome/bake-authoring.hpp"
#include <numeric>
#include <cstring>
namespace {
void domain_avatar_glb(const std::filesystem::path &prefix,const char *baseline,SurfaceRenderer &renderer,Fbo &fbo,frame_consumer::Renderer &consumer){
 Scene original;const auto group=append_humanoid(original,humanoid_baseline_defaults(baseline),"Retained-domain actual CC0 avatar",{},256);
 original.group(group).transform.scale=.8f;original.group(group).transform.position.y=-.7f;controlled_lighting(original,group);
 // Explicit native lighting edit: the actual baseline core fallback is unlit.
 // A normal-map sensitivity proof must use a lit material; raw sources remain intact.
 for(const auto &part:original.objects())if(part.group==group)original.object(part.id).material.unlit=false;
 auto face=std::find_if(original.objects().begin(),original.objects().end(),[&](const auto &o){return o.group==group&&o.name.find("Face_00_SKIN")!=std::string::npos;});
 check(face!=original.objects().end(),"Actual CC0 baseline provides the original FaceSkin normal field");const auto object=face->id;original.select(object);
 {auto &o=original.active();auto prepared=prepare_source_normals({o.painted,o.appearance,o.material_bindings,o.deformation,o.material});o.painted=prepared.paint;o.material_bindings=prepared.material_bindings;}
 // A genuine native normal-ray bake supplies detail, not a constant stand-in.
 auto high=original.active().painted->mesh();const auto artist=high.normals;
 for(size_t v=0;v<high.positions.size();++v){const auto p=high.positions[v];high.positions[v]+=artist[v]*(.003f+.0015f*std::sin(p.x*300.f)*std::sin(p.y*230.f));}
 high.normals.assign(high.positions.size(),{});
 for(size_t c=0;c<high.indices.size();c+=3){const auto a=high.indices[c],b=high.indices[c+1],d=high.indices[c+2];const auto normal=cross(high.positions[b]-high.positions[a],high.positions[d]-high.positions[a]);high.normals[a]+=normal;high.normals[b]+=normal;high.normals[d]+=normal;}
 for(size_t v=0;v<high.normals.size();++v)high.normals[v]=length(high.normals[v])>1e-12f?normalized(high.normals[v]):artist[v];
 const auto high_id=original.add("Readonly high FaceSkin",Volume{});auto &source=original.object(high_id);source.mesh_active=true;source.visible=false;source.locked=true;source.group=group;source.transform=original.object(object).transform;source.painted=std::make_shared<PaintedMesh>(std::move(high),256);
 ObjectBakeSettings settings;settings.front_mm=settings.back_mm=30;settings.ao_samples=0;settings.allow_partial=true;settings.padding=4;original.select(object);
 const auto report=bake_object(original,high_id,settings);(void)report;
 check(original.active().painted->baked()&&!original.active().painted->baked()->maps.normal.rgba.empty(),"Actual FaceSkin carries genuine native baked normal detail before retained-domain Scale");
 const auto exact=original.encode();const auto old_frames=original.active().material_bindings->normal_frames;
 SourceUvBakeInput input;input.paint=original.active().painted;input.current_material=original.active().material_bindings;input.current_deformation=original.active().deformation;input.appearance=original.active().appearance;input.target=input.paint->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
 for(auto &uv:input.target.uv)for(auto &q:uv)q=.5f+(q-.5f)*.95f;
 SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;auto adopted=adopt_source_uv_bake(bake_source_uv(input,options));
 auto current=original.snapshot();auto &target=current.active();target.painted=adopted.paint;target.material_bindings=adopted.material_bindings;target.deformation=adopted.deformation;target.appearance=adopted.appearance;
 check(target.material_bindings->normal_frames==old_frames,"Actual avatar Scale retains original authored frame identities without resampling them");
 auto project=prefix.string()+"-"+baseline+".dsculpt";current.save(project);auto loaded=Scene::load(project);check(loaded.encode()==current.encode(),"Actual nineteen-part avatar restarts with its complete Scale/source/detail graph");
 for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
  auto before=original,after=loaded;
  auto set_pose=[&](Scene &scene){for(const auto &item:scene.objects())if(item.group==group&&item.deformation){auto d=*item.deformation;d.pose.clear();for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);
    if(pose){const auto hips=d.humanoid->joints.at("hips"),spine=d.humanoid->joints.at("spine");d.pose[hips].scale={1.15f,.9f,.7f};d.pose[spine].scale={.85f,1.05f,1.2f};if(pose==2)d.pose[spine].rotation=Quat{std::cos(.08f),0,0,std::sin(.08f)};}
    if(d.morphs){d.morph_weights.assign(d.morphs->targets.size(),0);if(item.id==object)d.morph_weights[0]=weight;}scene.set_deformation(item.id,std::move(d));}};
  set_pose(before);set_pose(after);Document doc;doc.replace(after);check(doc.wait_mesh(std::chrono::seconds(120))&&doc.frame().state->error.empty(),"Actual avatar worker transports retained-domain attributes under current pose and morphs");
  doc.shading(false);check(doc.wait_mesh(std::chrono::seconds(120))&&doc.frame().state->pbr_view&&doc.frame().state->error.empty(),"Actual avatar PBR command publishes the selected immutable PBR frame role");
  const auto path=prefix.string()+"-"+baseline+"-p"+std::to_string(pose)+"-w"+std::to_string(int(weight*100))+".glb";
  doc.export_asset(path,AssetExportFormat::Glb).get();frame_consumer::Model parsed(path);
  check(parsed.data->meshes_count==19,"Independent retained-domain GLB keeps all nineteen genuine avatar parts");
  const auto native=key_draw(renderer,doc.frame().surface);fbo.bind();consumer.render(parsed.data,nullptr,{},true);const auto expected=read();const auto error=delta(native,expected);size_t covered=0;for(size_t at=3;at<expected.size();at+=4)covered+=expected[at]>0;
  std::cout<<"NORMAL_DOMAIN_AVATAR_GLB baseline="<<baseline<<" pose="<<pose<<" morph="<<weight<<" error="<<error<<" covered="<<covered<<'\n';
  check(covered>300&&error<=4,"Actual PBR viewport agrees with independent retained-domain avatar GLB across mixed joints and morphs");
  if(pose==0&&weight==0){
    const auto surfaces=after.preview_surfaces();const auto face=std::find_if(surfaces.begin(),surfaces.end(),[&](const auto &part){return part.id==object;});if(face==surfaces.end())throw std::runtime_error("Current avatar lacks prepared FaceSkin surface");
    const auto mesh=size_t(face-surfaces.begin());auto &normal=parsed.data->meshes[mesh].primitives[0].material->normal_texture;check(normal.texture&&normal.texcoord>0,"Genuine FaceSkin normal field uses its separately compiled UV domain");
    const auto saved=normal.scale;normal.scale=0;fbo.bind();consumer.render(parsed.data,nullptr,{},true);const auto sensitivity=delta(expected,read());normal.scale=saved;
    std::cout<<"NORMAL_DOMAIN_AVATAR_GLB_SENSITIVITY baseline="<<baseline<<" error="<<sensitivity<<'\n';check(sensitivity>2,"Independent actual-avatar pixels visibly depend on nonconstant retained normal detail");
    const auto saved_uv=normal.texcoord;normal.texcoord=0;fbo.bind();consumer.render(parsed.data,nullptr,{},true);const auto wrong=delta(expected,read());normal.texcoord=saved_uv;check(wrong>2,"Actual-avatar wrong-atlas normal sampling remains visibly incorrect");
  }
 }
 check(original.encode()==exact,"Actual avatar retained-domain authoring and consumers preserve original source/normal authority exactly");
}
}
int main(int argc,char **argv)try{
 if(argc<2)return 2;const std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';check(argc<3||std::string_view(argv[2])!="--hardware"||(device.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Actual avatar retained-domain fixture uses requested graphics device");Fbo fbo;fbo.bind();SurfaceRenderer renderer;frame_consumer::Renderer consumer;
 domain_avatar_glb(prefix,"vroid-beta-female",renderer,fbo,consumer);domain_avatar_glb(prefix,"vroid-beta-male",renderer,fbo,consumer);std::cout<<"PASS "<<checks<<" actual retained-domain core-PBR avatar checks\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
