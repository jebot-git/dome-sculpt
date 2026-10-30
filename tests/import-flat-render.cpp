// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "import-skin-fixtures.hpp"
#include "source-color-consumer.hpp"
namespace {
using color_consumer::attr;
Vec3 value(const cgltf_accessor *a,size_t v) {
  float x[4]{};
  if (!a || !cgltf_accessor_read_float(a,v,x,4)) throw std::runtime_error("Independent flat accessor missing");
  return {x[0],x[1],x[2]};
}
Vec3 face_normal(const std::array<Vec3,3> &p) {
  const double x=double(p[1].x)-p[0].x,y=double(p[1].y)-p[0].y,z=double(p[1].z)-p[0].z;
  const double u=double(p[2].x)-p[0].x,v=double(p[2].y)-p[0].y,w=double(p[2].z)-p[0].z;
  const double nx=y*w-z*v,ny=z*u-x*w,nz=x*v-y*u,m=std::sqrt(nx*nx+ny*ny+nz*nz);
  if (!m) throw std::runtime_error("Independent flat fixture is degenerate");
  return {float(nx/m),float(ny/m),float(nz/m)};
}
std::array<float,4> face_tangent(const std::array<Vec3,3> &p,const std::array<Vec3,3> &uv,Vec3 n) {
  auto a=p[1]-p[0],b=p[2]-p[0];
  double ux=double(uv[1].x)-uv[0].x,uy=double(uv[1].y)-uv[0].y;
  double vx=double(uv[2].x)-uv[0].x,vy=double(uv[2].y)-uv[0].y,d=ux*vy-uy*vx;
  auto du=(a*float(vy)-b*float(uy))/float(d),dv=(b*float(ux)-a*float(vx))/float(d);
  auto t=normalized(du-n*dot(n,du));
  return {t.x,t.y,t.z,dot(cross(n,t),dv)<0?-1.f:1.f};
}
void verify_frames(cgltf_data *source,cgltf_data *emitted) {
  const auto &raw=source->meshes[0].primitives[0];
  auto pos=cgltf_find_accessor(&raw,cgltf_attribute_type_position,0);
  auto uv=cgltf_find_accessor(&raw,cgltf_attribute_type_texcoord,0);
  auto dp=attr(raw.targets[0].attributes,raw.targets[0].attributes_count,cgltf_attribute_type_position);
  check(!cgltf_find_accessor(&raw,cgltf_attribute_type_normal,0),"Independent original GLB genuinely has no NORMAL accessor");
  bool normals=true,tangents=true;
  for(size_t m=0;m<emitted->meshes_count;++m) for(size_t p=0;p<emitted->meshes[m].primitives_count;++p) {
    const auto &out=emitted->meshes[m].primitives[p];
    auto n=cgltf_find_accessor(&out,cgltf_attribute_type_normal,0),t=cgltf_find_accessor(&out,cgltf_attribute_type_tangent,0);
    auto dn=attr(out.targets[0].attributes,out.targets[0].attributes_count,cgltf_attribute_type_normal);
    auto dt=attr(out.targets[0].attributes,out.targets[0].attributes_count,cgltf_attribute_type_tangent);
    check(out.indices && out.indices->count==raw.indices->count,"Export retains independent ordered triangle identities");
    for(size_t c=0;c<raw.indices->count;c+=3) {
      std::array<Vec3,3> base,target,tex;
      for(size_t k=0;k<3;++k) {
        auto v=cgltf_accessor_read_index(raw.indices,c+k);
        base[k]=value(pos,v);target[k]=base[k]+value(dp,v);tex[k]=value(uv,v);
      }
      const auto N=face_normal(base),nextN=face_normal(target);
      const auto T=face_tangent(base,tex,N),nextT=face_tangent(target,tex,nextN);
      for(size_t k=0;k<3;++k) {
        const auto v=cgltf_accessor_read_index(out.indices,c+k);
        normals &= length(value(n,v)-N)<2e-6f && length(value(dn,v)-(nextN-N))<2e-6f;
        float tangent[4]{};if(!t || !cgltf_accessor_read_float(t,v,tangent,4))throw std::runtime_error("Generated tangent missing");
        tangents &= length(value(t,v)-Vec3{T[0],T[1],T[2]})<2e-5f && tangent[3]==T[3] &&
          length(value(dt,v)-Vec3{nextT[0]-T[0],nextT[1]-T[1],nextT[2]-T[2]})<2e-5f;
      }
    }
  }
  check(normals,"Independent raw POSITION/target geometry proves exported base and target flat normals across creases");
  check(tangents,"Independent UV derivatives prove generated base/target tangents and ignored supplied handedness");
}
}
int main(int argc,char **argv) try {
  if(argc<2)return 2;
  const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
  NativeGraphics context;std::cout<<"GPU "<<glGetString(GL_RENDERER)<<std::endl;
  Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;
  for(bool authored:{false,true}) for(const char *alpha:{"OPAQUE","MASK","BLEND"}) {
    auto f=import_fixture::skinned(authored);
    auto &primitive=f.j["meshes"][0]["primitives"][0];primitive["attributes"].erase("NORMAL");primitive["targets"][0].erase("NORMAL");
    int positions=f.values({-.18f,-.18f,0,.18f,-.18f,.03f,.18f,.18f,.07f,-.18f,.18f,-.05f},3,"VEC3");
    f.j["accessors"][positions]["min"]={-.18f,-.18f,-.05f};f.j["accessors"][positions]["max"]={.18f,.18f,.07f};
    primitive["attributes"]["POSITION"]=positions;
    int delta_accessor=f.values({0,0,0,0,.02f,.04f,-.02f,0,.07f,0,0,-.03f},3,"VEC3");
    f.j["accessors"][delta_accessor]["min"]={-.02f,0,-.03f};f.j["accessors"][delta_accessor]["max"]={0,.02f,.07f};primitive["targets"][0]["POSITION"]=delta_accessor;
    auto &material=f.j["materials"][0];material["normalTexture"]={{"index",0},{"scale",.8f}};
    material["alphaMode"]=alpha;material["alphaCutoff"]=.3f;material.erase("extensions");
    const auto tag=std::to_string(authored)+"-"+alpha;
    auto path=directory/(tag+".glb");f.save(path);color_consumer::Model raw(path);
    ImportAuthoringOptions options;options.paint_resolution=64;auto prepared=prepare_authoring_import(Scene{},path,options);
    for(int stage=0;stage<2;++stage) for(float weight:{0.f,.35f,1.f}) {
      auto scene=prepared.scene.snapshot();
      for(auto id:prepared.objects) {auto state=*scene.object(id).deformation;state.morph_weights={weight};scene.set_deformation(id,state);}
      if(stage) {auto saved=directory/(tag+"-restart.dsculpt");scene.save(saved);scene=Scene::load(saved);}
      Document document;document.replace(scene);check(document.wait_mesh() && document.frame().state->error.empty(),"Real worker publishes generated flat frames with mixed skin and POSITION morphs");
      auto file=directory/(tag+"-"+std::to_string(stage)+"-"+std::to_string(int(weight*100))+"-out.glb");export_glb(scene.preview_surfaces(),file);
      color_consumer::Model emitted(file);verify_frames(raw.data,emitted.data);
      auto actual=draw(native,document.frame().surface);fbo.bind();independent.render(emitted.data);auto expected=read();
      int error=delta(actual,expected);std::cout<<"FLAT "<<tag<<" stage="<<stage<<" morph="<<weight<<" maxerror="<<error<<std::endl;
      check(error<=3,"Independent cgltf/PNG/PBR/normal-map/morph/skin consumer matches actual imported flat-normal pixels");
      size_t covered=0;for(size_t p=3;p<actual.size();p+=4)covered+=actual[p]!=0;
      check(covered>8,"Flat-normal render has nonempty geometry alpha coverage");
      for(size_t m=0;m<emitted.data->materials_count;++m)emitted.data->materials[m].normal_texture.scale=0;
      fbo.bind();independent.render(emitted.data);
      check(delta(expected,read())>1,"Independent removal of the normal texture changes actual visible pixels");
    }
  }
  std::cout<<checks<<" flat import consumer checks passed"<<std::endl;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
