// SPDX-License-Identifier: GPL-3.0-or-later
#define main established_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-paint.hpp"
#include "source4-baked.hpp"
#include "paint-overlay-packet.hpp"
#include <numeric>
namespace {
Image grid(std::array<uint8_t,4> q){Image a{64,64,std::vector<uint8_t>(4096*4)};for(size_t i=0;i<4096;++i)std::copy(q.begin(),q.end(),a.rgba.begin()+i*4);return a;}
Mesh varied(){auto q=quad();q.positions[0].z=.02f;q.positions[1].z=-.03f;q.positions[2].z=.06f;q.positions[3].z=-.02f;
 q.normals={normalized(Vec3{.1f,-.2f,1}),normalized(Vec3{.35f,.15f,1}),normalized(Vec3{-.25f,.4f,1}),normalized(Vec3{-.3f,-.1f,1})};Mesh m;
 for(auto i:q.indices){m.indices.push_back(uint32_t(m.positions.size()));m.positions.push_back(q.positions[i]);m.normals.push_back(q.normals[i]);m.uv.push_back(q.uv[i]);}return m;}
struct Sample{Vec3 detail{},radiance{};float common=0;};
Sample coefficients(float u,float v,float e){const double x=7.5+.75*(64*u),y=3.5+.875*(64*v);const int ix=int(std::floor(x)),iy=int(std::floor(y));const double fx=x-ix,fy=y-iy;Sample s;
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const int px=std::clamp(ix+dx,8,55),py=std::clamp(iy+dy,4,59);const float w=float((dx?fx:1-fx)*(dy?fy:1-fy)),a=(px<32?191:65)/255.f;const Vec3 q=px<32?Vec3{128,204,230}:Vec3{211,102,236};s.common+=w*a;s.detail+=(q/255.f*2-Vec3{1,1,1})*(w*a);s.radiance+=Vec3{e*(.15f+.001f*px),e*.03f,e*.004f*py}*w;
 }return s;}
struct Basis{Vec3 t,b;};
Basis derivative(const Mesh&m,size_t tri,const std::vector<std::array<float,2>>&uv,Vec3 N){const auto a=m.indices[tri*3],b=m.indices[tri*3+1],c=m.indices[tri*3+2];auto p=m.positions[b]-m.positions[a],q=m.positions[c]-m.positions[a];const auto x=uv[b],y=uv[c],z=uv[a];const float du=x[0]-z[0],dv=x[1]-z[1],eu=y[0]-z[0],ev=y[1]-z[1],det=du*ev-eu*dv;auto t=(p*ev-q*dv)/det,rawb=(q*du-p*eu)/det;t=normalized(t-N*dot(t,N));return {t,cross(N,t)*(dot(cross(N,t),rawb)<0?-1.f:1.f)};}
void run(bool typed_view,float emission_coverage){
 auto rest=varied(),atlas=rest;for(auto&uv:atlas.uv){uv[0]=.125f+.75f*uv[0];uv[1]=.0625f+.875f*uv[1];}
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image({255,255,255,255});maps->color.texcoord=1;maps->normal.image=image({156,128,250,255});maps->normal.texcoord=1;maps->normal_scale=.8f;maps->normal.uv_transform={.9f,.25f,-.15f,1.1f,.1f,-.2f};maps->emission.image=image({128,32,64,255});maps->emission.texcoord=1;
 auto original_uv=std::make_shared<std::vector<std::array<float,2>>>();for(auto uv:rest.uv)original_uv->push_back({.1f+.8f*uv[1],1-.7f*uv[0]-.2f*uv[1]});maps->uv1=original_uv;
 auto raw=PaintedMesh::from_maps(rest,maps,64);std::vector<uint32_t> ids(rest.positions.size());std::iota(ids.begin(),ids.end(),0);auto paint=prepare_source_paint(raw,atlas,ids);
 BakedTextures base;base.maps.color=grid({255,255,255,255});base.maps.metal_rough=grid({255,255,0,255});base.maps.normal=grid({128,204,230,255});base.coverage.resize(4096);std::vector<BakedEmissionSample> values(4096);
 for(size_t i=0;i<4096;++i){const int x=i%64,y=i/64;base.coverage[i]=x<32?191:65;if(x>=32){const std::array<uint8_t,4> q{211,102,236,255};std::copy(q.begin(),q.end(),base.maps.normal.rgba.begin()+i*4);}values[i]={{emission_coverage*.01f,0,0},{emission_coverage*(.15f+.001f*x),emission_coverage*.03f,emission_coverage*.004f*y},emission_coverage};}
 base.emission_field=BakedEmissionField::create(64,values);paint=detail::install_source_bake(*paint,std::move(base));paint->add_layer("Hidden actual physical data");PaintSettings ink;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={12000,.00001f,3};ink.front_only=false;Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=2;paint->begin_stroke();paint->stamp(brush,ink);check(paint->end_stroke(),"Actual Source8 hidden physical editable tile is nonempty");paint->layer_properties("Hidden actual physical data",false,.375f);
 Scene scene;auto&o=scene.active();o.mesh_active=true;o.painted=paint;o.material=white();o.material.unlit=false;o.material.metallic=0;o.material.roughness=1;o.material.emissive={.04f,.03f,.02f};
 std::shared_ptr<SourceMaterialBindings> binding;if(typed_view){binding=std::make_shared<SourceMaterialBindings>();binding->material=typed(false);binding->material.parameters.base_color={1,1,1,1};binding->material.parameters.shade_color={1,1,1};binding->material.parameters.emission={.04f,.03f,.02f};binding->material.parameters.emission_strength=3.5f;binding->native_reference.color={1,1,1,1};binding->native_reference.emission=o.material.emissive;binding->material.textures[size_t(MToonSlot::Emission)].index=0;binding->material.textures[size_t(MToonSlot::Emission)].texcoord=1;binding->material.textures[size_t(MToonSlot::Emission)].encoding=MToonEncoding::Srgb;binding->roles[size_t(MToonSlot::Emission)].image=maps->emission.image;o.material_bindings=binding;}
 const auto before=scene.encode();check(Scene::decode(before).encode()==before,"Visible varied-frame Source8/FLOAT native scene restarts exactly");Document doc;doc.replace(scene);if(typed_view)doc.authored_shading();else doc.shading(false);check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual Document publishes visible Source8 source/frame metadata");auto frame=doc.frame().surface;check(frame&&!frame->ranges.empty()&&detail::source_baked_preview_metadata(*frame->ranges[0].textures),"Actual packed Source8 frame owns its original roles and normal carriers");
 check(frame->ranges[0].textures->emission.rgba.empty()&&frame->ranges[0].textures->normal.rgba.empty()&&frame->tangents.size()==frame->mesh.positions.size(),"No flattened emission/composite-normal authority substitutes for actual current Mikk corners");
 Fbo f;SurfaceRenderer renderer;renderer.set_frame_time(0);f.bind();auto actual=draw(renderer,frame);
 auto normal_uv=*original_uv;for(auto&uv:normal_uv){const auto q=uv;uv={.9f*q[0]+.25f*q[1]+.1f,-.15f*q[0]+1.1f*q[1]-.2f};}
 for(const auto xy:std::array<std::array<int,2>,4>{{{24,30},{58,33},{62,64},{40,60}}}){
  const float x=(xy[0]+.5f)/96*2-1,y=(xy[1]+.5f)/96*2-1,u=(x+.75f)/1.5f,v=(y+.75f)/1.5f;const size_t tri=v<=u?0:1;const std::array<float,3> bary=tri==0?std::array<float,3>{1-u,u-v,v}:std::array<float,3>{1-v,u,v-u};Vec3 n{},ct{},cb{},position{};
  for(size_t k=0;k<3;++k){const auto i=rest.indices[tri*3+k];n+=rest.normals[i]*bary[k];position+=rest.positions[i]*bary[k];auto b=derivative(rest,tri,atlas.uv,rest.normals[i]);ct+=b.t*bary[k];cb+=b.b*bary[k];}
  const auto field=coefficients(u,v,emission_coverage);const auto N=normalized(n);auto original=N;if(!typed_view){const auto b=derivative(rest,tri,normal_uv,N);const auto ts=normalized(Vec3{(156.f/255*2-1)*.8f,(128.f/255*2-1)*.8f,250.f/255*2-1});original=normalized(b.t*ts.x+b.b*ts.y+N*ts.z);}const auto baked=normalized(ct*field.detail.x+cb*field.detail.y+n*field.detail.z);const auto desired=normalized(original*(1-field.common)+baked*field.common);
  const float strength=typed_view?3.5f:1;std::array<float,3> emitted{field.radiance.x,field.radiance.y,field.radiance.z};const std::array<float,3> background{.04f,.03f,.02f};const std::array<uint8_t,3> texture{128,32,64};for(size_t k=0;k<3;++k){const float q=srgb_to_linear(texture[k]/255.f)*background[k]*strength*(1-emission_coverage)+(k==0?emission_coverage*.01f*strength:0);emitted[k]+=q;}const Vec3 emission{emitted[0],emitted[1],emitted[2]};
  auto control=snapshot();for(auto&p:control->mesh.positions)p.z=position.z;control->mesh.normals.assign(4,desired);control->ranges[0].material=o.material;control->ranges[0].material.emissive={emission.x,emission.y,emission.z};if(typed_view){auto b=std::make_shared<SourceMaterialBindings>(*binding);b->material.textures[size_t(MToonSlot::Emission)].index=-1;b->roles[size_t(MToonSlot::Emission)]={};b->material.parameters.emission=emission;b->material.parameters.emission_strength=1;b->native_reference.emission=control->ranges[0].material.emissive;control->ranges[0].material_bindings=b;}
  f.bind();const auto expected=at(draw(renderer,control),xy[0],xy[1]),got=at(actual,xy[0],xy[1]);std::cout<<"SOURCE8_ORACLE typed="<<typed_view<<" coverage="<<emission_coverage<<" pixel="<<xy[0]<<","<<xy[1]<<" actual="<<got[0]<<","<<got[1]<<","<<got[2]<<" expected="<<expected[0]<<","<<expected[1]<<","<<expected[2]<<'\n';check(close(got,expected,3),"Actual visible Source8 agrees with independent varied role/current-frame and premultiplied coverage point oracle");
 }
 check(doc.copy_scene().encode()==before&&scene.encode()==before,"Visible Source8 display keeps exact original/FLOAT/hidden editable native owners");check(glGetError()==GL_NO_ERROR,"Source8 preview and independent controls leave the real GL error queue empty");
}
}
int main()try{NativeGraphics graphics;std::cout<<"ACTUAL_RENDERER "<<glGetString(GL_RENDERER)<<'\n';for(bool typed:{false,true})for(float coverage:{.25f,0x1p-24f})run(typed,coverage);std::cout<<"PASS "<<checks<<" Source8 actual visible normal/FLOAT/ownership controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
