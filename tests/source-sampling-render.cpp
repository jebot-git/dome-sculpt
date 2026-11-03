// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "../src/source-sampling-transport.hpp"
#include "source-color-consumer.hpp"
#include "dome/gltf-export.hpp"

namespace {
std::shared_ptr<const Image> checker(uint32_t size) {
  Image out{size,size,std::vector<uint8_t>(size_t(size)*size*4)};
  for(uint32_t y=0;y<size;++y)for(uint32_t x=0;x<size;++x){const size_t at=(size_t(y)*size+x)*4;
    const bool bright=((x+y)&1)==0;out.rgba[at]=bright?255:0;
    out.rgba[at+1]=bright?0:255;out.rgba[at+2]=bright?64:192;out.rgba[at+3]=255;}
  return std::make_shared<const Image>(std::move(out));
}
std::shared_ptr<MeshSnapshot> model(const Mesh &mesh,std::shared_ptr<const PaintSourceMaps> maps,
                                    const Material &material) {
  auto result=std::make_shared<MeshSnapshot>();result->mesh=mesh;
  auto textures=std::make_shared<TextureMaps>();textures->source_only=true;textures->source_maps=std::move(maps);
  result->ranges.push_back({1,0,mesh.indices.size(),material,textures,{},{}});return result;
}
void exported(const std::filesystem::path &path,const Mesh &mesh,
              std::shared_ptr<const PaintSourceMaps> maps,const Material &material) {
  ObjectSurface surface;surface.id=1;surface.mesh=mesh;surface.material=material;
  auto textures=std::make_shared<TextureMaps>();textures->source_only=true;textures->source_maps=maps;surface.textures=textures;
  export_glb({surface},path);color_consumer::Model parsed(path);
  check(cgltf_validate(parsed.data)==cgltf_result_success,"Independent cgltf validates actual sampling-carrier GLB");
  const auto &primitive=parsed.data->meshes[0].primitives[0];const auto &m=*primitive.material;
  const std::array<const cgltf_texture_view*,5> views{&m.pbr_metallic_roughness.base_color_texture,
    &m.pbr_metallic_roughness.metallic_roughness_texture,&m.emissive_texture,&m.normal_texture,&m.occlusion_texture};
  const std::array<const PaintSourceTexture*,5> sources{&maps->color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->occlusion};
  for(size_t role=0;role<views.size();++role) {
    const auto &view=*views[role];const auto &source=*sources[role];
    check(view.texture&&view.texture->sampler&&
          view.texture->sampler->min_filter==source.min_filter&&view.texture->sampler->mag_filter==source.mag_filter&&
          view.texture->sampler->wrap_s==source.wrap_s&&view.texture->sampler->wrap_t==source.wrap_t,
          "Actual exported role retains its original min/mag/wrap sampler");
    check(color_consumer::image(view.texture).rgba==source.image->rgba,
          "Independent PNG decoder reads unchanged original role pixels");
    const int coordinate=view.has_transform&&view.transform.has_texcoord?view.transform.texcoord:view.texcoord;
    const auto *accessor=color_consumer::attr(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_texcoord,coordinate);
    check(accessor!=nullptr,"Actual exported role resolves its own explicit coordinate accessor");
    for(size_t c=0;c<mesh.indices.size();++c) {
      const size_t vertex=mesh.indices[c];float q[2];
      check(cgltf_accessor_read_float(accessor,vertex,q,2),"Independent reader resolves transported corner coordinates");
      double u=q[0],v=q[1];
      if(view.has_transform) {
        const auto &t=view.transform;const double a=u*t.scale[0],b=v*t.scale[1];
        u=std::cos(t.rotation)*a-std::sin(t.rotation)*b+t.offset[0];
        v=std::sin(t.rotation)*a+std::cos(t.rotation)*b+t.offset[1];
      }
      const auto original=source_texture_coordinates(mesh,*maps,source.texcoord)[vertex];
      const auto &t=source.uv_transform;
      check(std::abs(u-(t[0]*original[0]+t[1]*original[1]+t[4]))<1e-6&&
            std::abs(v-(t[2]*original[0]+t[3]*original[1]+t[5]))<1e-6,
            "Independent exported sampling coordinate equals the exact original affine role domain");
    }
  }
}
}
int main(int argc,char **argv)try {
  if(argc<2)return 2;NativeGraphics context;
  const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  const std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GL_VENDOR "<<vendor<<" GL_RENDERER "<<gpu<<'\n';
  check(argc<3||std::string_view(argv[2])!="--hardware"||
        (vendor.find("Intel")!=std::string::npos&&gpu.find("Arc")!=std::string::npos&&gpu.find("A770")!=std::string::npos),
        "Requested hardware fixture requires actual Intel Arc A770");
  const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
  Mesh before=quad();before.uv={{-1.25f,-.5f},{1.75f,-.5f},{1.75f,2.5f},{-1.25f,2.5f}};
  const std::vector<uint32_t> vertices{0,1,2,0,2,3};Mesh target;
  for(auto v:vertices){target.positions.push_back(before.positions[v]);target.normals.push_back(before.normals[v]);}
  target.indices={0,1,2,3,4,5};target.uv={{.05f,.05f},{.45f,.05f},{.45f,.45f},{.55f,.55f},{.95f,.95f},{.55f,.95f}};
  Fbo fbo;fbo.bind();SurfaceRenderer renderer;int maximum=0;size_t cases=0;
  for(uint32_t size:{2u,128u})for(int min:{9728,9729,9984,9985,9986,9987})for(int mag:{9728,9729})
  for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}) {
    auto source=std::make_shared<PaintSourceMaps>();auto picture=checker(size);
    source->uv1=std::make_shared<const std::vector<std::array<float,2>>>(before.uv);
    source->extra_uv_sets[2]=source->uv1;source->extra_uv_sets[4]=source->uv1;
    const std::array<PaintSourceTexture*,5> slots{&source->color,&source->metal_rough,&source->emission,&source->normal,&source->occlusion};
    for(size_t role=0;role<slots.size();++role) {
      auto &view=*slots[role];view.image=role==3?image({210,160,235,255}):picture;
      view.texcoord=std::array<int,5>{0,1,4,2,0}[role];view.min_filter=min;view.mag_filter=mag;
      view.wrap_s=33648;view.wrap_t=10497;
      view.uv_transform={.8f,0,0,1.1f,.13f,-.27f};
    }
    const auto maps=detail::transport_source_sampling(before,*source,target,vertices);
    Material material;material.unlit=false;material.color={.7f,.8f,.9f,.75f};
    material.emissive={.1f,.07f,.03f};material.metallic=.3f;material.roughness=.7f;material.alpha=alpha;
    const auto raw=draw(renderer,model(before,source,material));const auto transported=draw(renderer,model(target,maps,material));
    size_t visible=0;for(size_t i=3;i<raw.size();i+=4)visible+=raw[i]!=0;
    check(visible>128,"Sampling preservation compares substantial actual visible geometry");
    const int error=delta(raw,transported);maximum=std::max(maximum,error);
    std::cout<<"SAMPLING_IMAGE size="<<size<<" min="<<min<<" mag="<<mag<<" alpha="<<int(alpha)<<" maxerror="<<error<<'\n';
    check(error<=2,"Independent original and atlas sampling domains match under actual magnification/minification");
    auto path=directory/(std::to_string(size)+"-"+std::to_string(min)+"-"+std::to_string(mag)+"-"+std::to_string(int(alpha))+".glb");
    exported(path,target,maps,material);++cases;
  }
  std::cout<<"Original sampling graphics "<<checks<<" checks / "<<cases<<" cases; maxerror="<<maximum<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
