// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/creation.hpp"
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"
#include "dome/vrm.hpp"
#include "dome/projection.hpp"
#include "vrm-role-consumer.hpp"
#include "vrm-derivative-fixture.hpp"
#include <set>
namespace {
Image solid(uint32_t r,std::array<uint8_t,4> c){Image out{r,r,std::vector<uint8_t>(size_t(r)*r*4)};for(size_t i=0;i<out.rgba.size();++i)out.rgba[i]=c[i%4];return out;}
uint8_t byte(float x){return uint8_t(std::lround(std::clamp(x,0.f,1.f)*255));}
ImportedAuthoring authored_roles(const std::filesystem::path &path,bool expression){
 const auto base=create_humanoid(humanoid_baseline_defaults("vroid-beta-female"));
 vrm_derivative_fixture::Source source(*base.parts.front().appearance->original_bytes);
 auto &json=source.json;
 if(json.at("textures").size()<2)throw std::runtime_error("Real avatar needs two historical texture identities");
 json["textures"][1]=json["textures"][0];
 for(auto &property:json["extensions"]["VRM"]["materialProperties"]){
  auto m=MToonMaterial::from_vrm0(property);for(auto &t:m.textures){t.index=-1;t.offset={};t.scale={1,1};}
  m.textures[0].index=0;m.textures[1].index=1;m.parameters.normal_enabled=false;m.parameters.normal_scale=0;
  m.parameters.base_color={.75f,.75f,.75f,1};m.parameters.shade_color={.18f,.12f,.08f};m.parameters.toony=1;m.parameters.shift=1;
  m.parameters.indirect_intensity=0;m.parameters.light_color_attenuation=0;m.parameters.emission={};m.parameters.rim_color={};m.parameters.outline=MToonOutline::None;
  m.parameters.scroll={};m.parameters.rotation_speed=0;m.parameters.cull=MToonCull::Back;m.parameters.alpha=MToonAlpha::Mask;m.parameters.alpha_cutoff=.35f;
  m.parameters.depth_write=true;m.parameters.source_blend=1;m.parameters.destination_blend=0;m.parameters.render_queue=2450;property=m.source_json();
 }
 if(expression){auto values=nlohmann::json::array();std::set<int> used;for(const auto &mesh:json.at("meshes"))for(const auto &primitive:mesh.at("primitives"))used.insert(primitive.at("material").get<int>());for(int material:used){auto name=json["extensions"]["VRM"]["materialProperties"].at(size_t(material))["name"];values.push_back({{"materialName",name},{"propertyName","_Color"},{"targetValue",{.4f,.65f,.8f,1}}});values.push_back({{"materialName",name},{"propertyName","_ShadeColor"},{"targetValue",{.25f,.4f,.55f,1}}});}json["extensions"]["VRM"]["blendShapeMaster"]["blendShapeGroups"].push_back({{"name","AliasFactor"},{"presetName","unknown"},{"binds",nlohmann::json::array()},{"materialValues",std::move(values)},{"isBinary",false}});}
 return source.import(path);
}
void set_preview_materials(Scene &scene,uint64_t group) {
 for(const auto &part:scene.objects())if(vrm_derivative_fixture::in_avatar(scene,part,group)){
  auto &object=scene.object(part.id);
  object.material.color=object.material_bindings->native_reference.color;
  object.material.emissive={};object.material.alpha=Material::AlphaMode::Mask;
  object.material.alpha_cutoff=.35f;object.material.double_sided=false;
 }
 scene.validate();
}
void role_images(Scene &scene,uint64_t group,bool animated){
 auto source=animated?std::make_shared<const Image>(Image{2,2,{64,128,224,255,224,80,128,255,160,224,96,255,240,192,64,255}}):image({128,192,224,255});
 for(const auto &item:scene.objects())if(vrm_derivative_fixture::in_avatar(scene,item,group)){auto &o=scene.object(item.id);auto b=std::make_shared<SourceMaterialBindings>(*o.material_bindings);for(size_t role=0;role<2;++role){auto &v=b->roles[role];v.image=source;v.wrap_s=v.wrap_t=10497;v.min_filter=v.mag_filter=9729;}if(animated)b->material.parameters.scroll={.25f,.05f};b->validate();o.material_bindings=b;}
}
void pose(Scene &scene,uint64_t group,float weight){for(const auto &item:scene.objects())if(vrm_derivative_fixture::in_avatar(scene,item,group)){auto d=*item.deformation;d.pose.clear();for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);if(weight>=0)d.pose[d.humanoid->joints.at("hips")].scale={1.5f,1,.7f};if(d.morphs){d.morph_weights.assign(d.morphs->targets.size(),0);if(!d.morph_weights.empty())d.morph_weights[0]=std::max(weight,0.f);}scene.set_deformation(item.id,std::move(d));}}
struct Overlay {std::shared_ptr<MeshSnapshot> snapshot;std::vector<ObjectSurface> objects;};
Overlay overlay(Scene &scene,uint64_t group,int mode,bool raw=false){
 Document document;document.replace(scene);if(!document.wait_mesh(std::chrono::seconds(30)))throw std::runtime_error("Actual avatar worker failed");
 Overlay out{std::make_shared<MeshSnapshot>(*document.frame().surface),scene.preview_surfaces()};std::erase_if(out.objects,[&](const auto&o){return !vrm_derivative_fixture::in_avatar(o,group);});
 for(auto &o:out.objects){auto maps=std::make_shared<TextureMaps>();auto source=std::make_shared<PaintSourceMaps>();source->color.image=image({128,160,192,255});source->color.wrap_s=source->color.wrap_t=33071;source->color.min_filter=source->color.mag_filter=9729;maps->source_maps=source;
  maps->color=solid(64,{0,0,0,255});maps->metal_rough=solid(64,{255,128,0,255});maps->emission=solid(64,{0,0,0,255});maps->color_coverage=solid(64,{0,0,0,255});maps->emission_coverage=solid(1,{0,0,0,255});
  auto base=sample_source_texture(source->color,.5f,.5f,true);
  for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){size_t at=(y*64+x)*4;uint8_t c=mode==2?255:mode==0?0:(x<8||x>=56)?0:(x<22||x>=42)?128:255;maps->color_coverage.rgba[at]=c;for(size_t k=0;k<4;++k){const float paint=std::array<float,4>{.45f,.8f,.25f,1}[k];const float value=o.material.color[k]*base[k]*(1-c/255.f)+paint*(c/255.f);maps->color.rgba[at+k]=byte(k<3?linear_to_srgb(value):value);}}
  o.textures=maps;if(raw)o.material_bindings.reset();for(auto &r:out.snapshot->ranges)if(r.object==o.id){r.textures=maps;if(raw)r.material_bindings.reset();}
 }
 return out;
}
void exact_positions(const Overlay &input,const cgltf_data &file,const role_consumer::Renderer &consumer){float error=0;size_t corners=0;for(size_t r=0;r<input.snapshot->ranges.size();++r){const auto &range=input.snapshot->ranges[r];const auto &p=file.meshes[r].primitives[0];for(size_t c=0;c<range.count;++c){auto v=cgltf_accessor_read_index(p.indices,c);error=std::max(error,length(input.snapshot->mesh.positions[input.snapshot->mesh.indices[range.first+c]]-consumer.world_positions.at(r).at(v)));++corners;}}check(file.meshes_count==19 && corners>10000 && error<1e-5f,"Independent parsed nineteen-part geometry and skin/morph pose match the actual native worker");}
void apply_expression(role_consumer::Model &model,float weight){auto &vrm=model.json["extensions"]["VRM"];bool found=false;for(const auto &g:vrm["blendShapeMaster"]["blendShapeGroups"])if(g["name"]=="AliasFactor"){for(const auto &value:g["materialValues"])for(auto &m:vrm["materialProperties"])if(m["name"]==value["materialName"]){auto &base=m["vectorProperties"][value["propertyName"].get<std::string>()];for(size_t c=0;c<4;++c)base[c]=float(base[c])*(1-weight)+float(value["targetValue"][c])*weight;found=true;}}if(!found)throw std::runtime_error("Actual exported VRM lost the authored material-value expression");}
void native_expression(Scene &scene,uint64_t group,float weight){for(const auto &item:scene.objects())if(vrm_derivative_fixture::in_avatar(scene,item,group)){auto &o=scene.object(item.id);auto binding=std::make_shared<SourceMaterialBindings>(*o.material_bindings);for(size_t c=0;c<3;++c){const float base=.75f;auto gamma=[](float x){return x<=.0031308f?x*12.92f:1.055f*std::pow(x,1/2.4f)-.055f;};const float target=std::array<float,3>{.4f,.65f,.8f}[c];const float effective=role_consumer::linear(gamma(base)*(1-weight)+target*weight);const float reference=binding->native_reference.color[c];o.material.color[c]=reference?reference*effective/base:weight==0?0:effective;const float old_shade=std::array<float,3>{.18f,.12f,.08f}[c];const float new_shade=role_consumer::linear(gamma(old_shade)*(1-weight)+std::array<float,3>{.25f,.4f,.55f}[c]*weight);if(c==0)binding->material.parameters.shade_color.x=new_shade;else if(c==1)binding->material.parameters.shade_color.y=new_shade;else binding->material.parameters.shade_color.z=new_shade;}o.material_bindings=binding;}}
}
int main(int argc,char **argv)try{
 if(argc<2)return 2;std::filesystem::path prefix=argv[1];NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';check(argc<3||std::string(argv[2])!="--hardware"||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),"Resolved-role regression executes on the requested graphics device");Fbo framebuffer;framebuffer.bind();SurfaceRenderer native;role_consumer::Renderer consumer;
 auto input=prefix;input+="-alias-source.vrm";auto imported=authored_roles(input,false);const auto group=vrm_derivative_fixture::root(imported);
 check(imported.objects.size()==19,"Native import adopts the external raw MToon role aliases");
 auto base=std::move(imported.scene);vrm_derivative_fixture::presentation(base,group);set_preview_materials(base,group);role_images(base,group,false);
 for(float weight:{0.f,.35f,1.f}){auto scene=base;pose(scene,group,weight);auto inputs=overlay(scene,group,1);auto path=prefix;path+="-resolved-"+std::to_string(int(weight*100))+".vrm";VrmExportOptions options;options.paint_resolution=64;export_vrm(inputs.objects,path,options);role_consumer::Model file(path);auto expected=draw(native,inputs.snapshot);framebuffer.bind();consumer.render(file.data,&file.json["extensions"]["VRM"],*inputs.objects[0].appearance->source_basis_quaternion);int error=delta(expected,read());std::cout<<"RESOLVED_ALIAS morph="<<weight<<" maxerror="<<error<<'\n';check(error<=4,"Current role image/sampler/UV aliases retain protected, fractional and full shade paint despite different historical indices");exact_positions(inputs,*file.data,consumer);}
 {auto scene=base;for(const auto &item:scene.objects())if(vrm_derivative_fixture::in_avatar(scene,item,group)){auto &o=scene.object(item.id);auto b=std::make_shared<SourceMaterialBindings>(*o.material_bindings);for(size_t role=0;role<2;++role){const auto &t=o.appearance->asset->textures[role];auto &v=b->roles[role];v.image=o.appearance->decoded_images->at(size_t(t.image));v.wrap_s=t.wrap_s;v.wrap_t=t.wrap_t;v.min_filter=t.min_filter;v.mag_filter=t.mag_filter;}o.material_bindings=b;}pose(scene,group,-1);auto inputs=overlay(scene,group,2,true);auto path=prefix;path+="-raw.vrm";VrmExportOptions options;options.paint_resolution=64;export_vrm(inputs.objects,path,options);role_consumer::Model file(path);auto expected=draw(native,inputs.snapshot);framebuffer.bind();consumer.render(file.data,&file.json["extensions"]["VRM"],*inputs.objects[0].appearance->source_basis_quaternion);int error=delta(expected,read());std::cout<<"RAW_NONALIAS maxerror="<<error<<'\n';check(error<=4,"Raw materials keep their distinct descriptor-role semantics when images match but no resolved holder exists");exact_positions(inputs,*file.data,consumer);}
 auto animated_input=prefix;animated_input+="-animated-source.vrm";auto external=authored_roles(animated_input,true);
 const auto animated_group=vrm_derivative_fixture::root(external);auto animated=std::move(external.scene);
 vrm_derivative_fixture::presentation(animated,animated_group);set_preview_materials(animated,animated_group);role_images(animated,animated_group,true);pose(animated,animated_group,.35f);auto original=overlay(animated,animated_group,0);auto path=prefix;path+="-animated.vrm";VrmExportOptions options;options.paint_resolution=64;export_vrm(original.objects,path,options);
 Pixels old;
 for(float weight:{0.f,.35f,1.f})for(double seconds:{0.,.7}){role_consumer::Model file(path);apply_expression(file,weight);auto scene=animated;native_expression(scene,animated_group,weight);auto inputs=overlay(scene,animated_group,0);native.set_frame_time(seconds);consumer.seconds=seconds;auto expected=draw(native,inputs.snapshot);framebuffer.bind();consumer.render(file.data,&file.json["extensions"]["VRM"],*inputs.objects[0].appearance->source_basis_quaternion);int error=delta(expected,read());std::cout<<"PROTECTED_ANIMATION factor="<<weight<<" seconds="<<seconds<<" maxerror="<<error<<'\n';check(error<=4,"Zero common replacement preserves actual material-value expression animation and original moving UV roles");if(weight==0 && seconds==0)old=expected;else check(expected!=old,"Animated UV or material factors change real native pixels instead of passing a constant baseline");}
 std::cout<<"Actual VRM role identity checks "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
