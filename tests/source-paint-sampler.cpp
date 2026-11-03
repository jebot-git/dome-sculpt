// SPDX-License-Identifier: GPL-3.0-or-later
#define main established_graphics_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/source-paint.hpp"
namespace {
std::shared_ptr<const Image> pattern(uint32_t n){Image im{n,n,std::vector<uint8_t>(size_t(n)*n*4)};for(uint32_t y=0;y<n;++y)for(uint32_t x=0;x<n;++x){size_t i=(size_t(y)*n+x)*4;bool a=(x+y)%2;im.rgba[i]=a?255:0;im.rgba[i+1]=a?0:255;im.rgba[i+2]=a?64:192;im.rgba[i+3]=a?255:80;}return std::make_shared<const Image>(std::move(im));}
std::shared_ptr<MeshSnapshot> model(const Mesh &m,std::shared_ptr<const TextureMaps> textures,const Material &material){auto out=std::make_shared<MeshSnapshot>();out->mesh=m;out->ranges.push_back({1,0,m.indices.size(),material,textures,{},0});return out;}
}
int main(int argc,char **argv)try{
 NativeGraphics graphics;std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';check(argc<2||std::string_view(argv[1])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Hardware mode requires actual A770");
 auto raw=quad();raw.uv={{-1.25f,-.5f},{1.75f,-.5f},{1.75f,2.5f},{-1.25f,2.5f}};
 const std::vector<uint32_t> vertices{0,1,2,0,2,3};Mesh target;for(auto v:vertices){target.positions.push_back(raw.positions[v]);target.normals.push_back(raw.normals[v]);}target.indices={0,1,2,3,4,5};target.uv={{.05f,.05f},{.45f,.05f},{.45f,.45f},{.55f,.55f},{.95f,.95f},{.55f,.95f}};
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;int maximum=0;size_t cases=0;
 for(uint32_t size:{2u,128u})for(int min:{9728,9729,9984,9985,9986,9987})for(int mag:{9728,9729})for(auto mode:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
  auto source=std::make_shared<PaintSourceMaps>();source->uv1=std::make_shared<const std::vector<std::array<float,2>>>(raw.uv);source->extra_uv_sets[2]=source->uv1;source->extra_uv_sets[4]=source->uv1;const auto picture=pattern(size);
  const std::array<PaintSourceTexture*,5> roles{&source->color,&source->metal_rough,&source->emission,&source->normal,&source->occlusion};for(size_t role=0;role<roles.size();++role){auto &v=*roles[role];v.image=role==3?image({210,160,235,255}):picture;v.texcoord=std::array<int,5>{0,1,4,2,0}[role];v.min_filter=min;v.mag_filter=mag;v.wrap_s=33648;v.wrap_t=10497;v.uv_transform={.8f,0,0,1.1f,.13f,-.27f};}
  PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;auto before=PaintedMesh::from_maps(raw,source,64,policy);const auto baseline=before->encode();auto paint=prepare_source_paint(before,target,vertices);
  PaintSettings setting;setting.channel=PaintChannel::Emission;setting.emission={0,0,0};setting.front_only=false;Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=2;brush.strength=.5f;paint->begin_stroke();paint->stamp(brush,setting);check(paint->end_stroke(),"Actual allocated paint enables the full sparse overlay path");
  Material material;material.color={.7f,.8f,.9f,.75f};material.metallic=.3f;material.roughness=.7f;material.alpha=mode;
  auto original=std::make_shared<TextureMaps>();original->source_maps=source;original->source_only=true;auto overlay=paint->preview(material);check(bool(overlay->overlay),"Nonzero emission coverage produces an immutable actual overlay");
  const auto expected=draw(renderer,model(raw,original,material)),actual=draw(renderer,model(target,overlay,material));const int error=delta(expected,actual);maximum=std::max(maximum,error);std::cout<<"SAMPLER size="<<size<<" min="<<min<<" mag="<<mag<<" alpha="<<int(mode)<<" max="<<error<<'\n';
  check(error<=2,"Active native overlay retains original per-role sampler, image and affine UV appearance");check(before->encode()==baseline,"Original source remains exact through actual overlay GPU execution");++cases;
 }
 std::cout<<"PASS "<<cases<<" actual overlay sampler cases "<<checks<<" checks maxerror="<<maximum<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
