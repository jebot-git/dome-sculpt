// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_fbx_deformation_suite
#include "fbx-deformation.cpp"
#undef main
namespace {
ufbx_vec3 difference(ufbx_vec3 a,ufbx_vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
ufbx_vec3 cross(ufbx_vec3 a,ufbx_vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
ufbx_vec3 unit(ufbx_vec3 n){auto length=std::hypot(n.x,n.y,n.z);check(length>0&&std::isfinite(length),"Independent target has a finite nonzero geometric normal");return {n.x/length,n.y/length,n.z/length};}
void add(ufbx_vec3 &a,ufbx_vec3 b){a.x+=b.x;a.y+=b.y;a.z+=b.z;}
std::vector<const ufbx_blend_shape*> source_shapes(const ufbx_mesh &mesh){
 std::vector<const ufbx_blend_shape*> result;
 for(auto *d:mesh.blend_deformers)for(auto *c:d->channels)for(auto k:c->keyframes)result.push_back(k.shape);
 return result;
}
double maximum_normal_error=0,maximum_delta=0;
size_t normal_corners=0;
void compare_normals(const ImportedAsset &asset,const ufbx_scene &source,ImportNormals mode){
 for(const auto &p:asset.primitives){
  const auto &mesh=*source.nodes[size_t(p.source_node)]->mesh;
  const auto shapes=source_shapes(mesh);
  check(shapes.size()==p.morph_targets.size(),"Every source morph endpoint remains independently editable");
  const auto controls=nlohmann::json::parse(p.extras_json).at("dome_source_vertices").get<std::vector<uint32_t>>();
  for(size_t key=0;key<shapes.size();++key){
   const auto &shape=*shapes[key];const auto &target=p.morph_targets[key];
   check(target.normals.size()==p.mesh.positions.size(),"Generated normal deltas cover every canonical target corner");
   std::vector<ufbx_vec3> points(mesh.vertices.begin(),mesh.vertices.end()),smooth(mesh.num_vertices);
   // Independent sparse-control evaluation: enumerate the original offsets,
   // rather than calling the producer's indexed offset lookup.
   for(size_t i=0;i<shape.num_offsets;++i){auto delta=shape.position_offsets[i];double weight=i<shape.offset_weights.count?shape.offset_weights[i]:1;
    delta.x*=weight;delta.y*=weight;delta.z*=weight;add(points.at(shape.offset_vertices[i]),delta);}
   if(mode==ImportNormals::GenerateSmooth)for(auto face:mesh.faces){
    ufbx_vec3 area{};
    // Newell face integration is independent of the producer's ufbx normal call.
    for(size_t c=0;c<face.num_indices;++c){const auto a=points.at(mesh.vertex_indices[face.index_begin+c]),b=points.at(mesh.vertex_indices[face.index_begin+(c+1)%face.num_indices]);
     area.x+=(a.y-b.y)*(a.z+b.z);area.y+=(a.z-b.z)*(a.x+b.x);area.z+=(a.x-b.x)*(a.y+b.y);}
    for(size_t c=0;c<face.num_indices;++c)add(smooth.at(mesh.vertex_indices[face.index_begin+c]),area);
   }
   for(size_t t=0;t<p.mesh.indices.size();t+=3){
    const auto i=p.mesh.indices[t],j=p.mesh.indices[t+1],k=p.mesh.indices[t+2];
    ufbx_vec3 flat{};if(mode==ImportNormals::GenerateFlat)flat=unit(cross(difference(points.at(controls[j]),points.at(controls[i])),difference(points.at(controls[k]),points.at(controls[i]))));
    for(size_t c=0;c<3;++c){const auto v=p.mesh.indices[t+c];const auto expected=mode==ImportNormals::GenerateSmooth?unit(smooth.at(controls[v])):flat;
     const auto base=p.mesh.normals.at(v),delta=target.normals.at(v);
     const auto actual=unit({double(base.x)+delta.x,double(base.y)+delta.y,double(base.z)+delta.z});
     maximum_normal_error=std::max({maximum_normal_error,std::abs(expected.x-actual.x),std::abs(expected.y-actual.y),std::abs(expected.z-actual.z)});
     maximum_delta=std::max(maximum_delta,double(length(delta)));
     const auto position=p.mesh.positions[v]+target.positions[v];const auto ref=points.at(controls[v]);
     check(std::max({std::abs(position.x-ref.x),std::abs(position.y-ref.y),std::abs(position.z-ref.z)})<3e-5,
       "Canonical morph position remains the independent original sparse shape endpoint");
     ++normal_corners;
    }
   }
  }
 }
 check(maximum_normal_error<2e-6,"Every generated morph normal matches the independent geometric endpoint");
}
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--preserve"){
  std::filesystem::create_directories(argv[2]);
  for(const auto *name:{"maya_blend_shape_cube_7700_ascii.fbx","blender_279_shape_weights_7400_binary.fbx"}){
   auto captured=capture_native_source(std::filesystem::path(DOME_FBX_FIXTURE_DIRECTORY)/name);
   std::ofstream file(std::filesystem::path(argv[2])/(std::string(name)+"-preserve.glb"),std::ios::binary);
   file.write(reinterpret_cast<const char*>(captured.canonical_glb->data()),std::streamsize(captured.canonical_glb->size()));
  }
  return 0;
 }
 check(argc==2,"Choose a private generated-normal output directory");const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 for(const auto *name:{"maya_blend_shape_cube_7700_ascii.fbx","blender_279_shape_weights_7400_binary.fbx"}){
  const auto path=std::filesystem::path(DOME_FBX_FIXTURE_DIRECTORY)/name;auto source=load_reference(path);
  auto pristine=capture_native_source(path);std::ofstream raw(out/(std::string(name)+"-preserve.glb"),std::ios::binary);raw.write(reinterpret_cast<const char*>(pristine.canonical_glb->data()),std::streamsize(pristine.canonical_glb->size()));raw.close();
  for(auto mode:{ImportNormals::GenerateFlat,ImportNormals::GenerateSmooth}){
   std::cout<<"CASE "<<name<<" normals="<<int(mode)<<std::endl;
   ImportOptions choice;choice.normals=mode;auto captured=capture_native_source(path,choice);
   check(std::string(captured.original->root.bytes->begin(),captured.original->root.bytes->end())==bytes(path),"Exact source bytes remain retained after generated morph normal conversion");
   check(captured.original->adapter.find("adapter 5")!=std::string::npos,"New normal conversion has an explicit adapter identity");
   auto asset=import_glb(*captured.canonical_glb);compare_normals(asset,*source,mode);
   const auto tag=std::string(name)+"-"+std::to_string(int(mode));std::ofstream file(out/(tag+"-canonical.glb"),std::ios::binary);file.write(reinterpret_cast<const char*>(captured.canonical_glb->data()),std::streamsize(captured.canonical_glb->size()));file.close();
   ImportAuthoringOptions options;options.choices=choice;options.paint_resolution=64;
   auto imported=prepare_authoring_import(Scene{},path,options);compare_source(imported.scene,imported.objects,*source);
   const auto endpoint=imported.scene.encode();restart(imported.scene,out/(tag+".dsculpt"));compare_export(imported.scene,imported.objects,out/(tag+".glb"));
   check(imported.scene.encode()==endpoint,"Owning import, native restart and independent export preserve the complete endpoint");
  }
 }
 check(maximum_delta>.01&&normal_corners>100,"Genuine source morphs exercise a significant normal change rather than zero deltas");
 std::cout<<"PASS "<<checks<<" generated FBX morph checks; "<<normal_corners<<" independent normal corners, max error "<<maximum_normal_error<<", largest delta "<<maximum_delta<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
