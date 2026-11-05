// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-render.hpp"
#include "polygon-view-state.hpp"
#include "dome/render.hpp"
#include <nlohmann/json.hpp>
#ifndef DOME_AFFINE_PREVIEW_ONLY
#include "dome/source-placement.hpp"
#include "polygon-fixture.hpp"
#include "source-color-fixtures.hpp"
#endif
#include "graphics.h"
#include <EGL/eglext.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <fstream>
#include <png.h>
#include <iostream>
#include <set>
using namespace dome;
namespace {
unsigned checks=0;bool capture=false;size_t uploaded=0,readbacks=0;
void check(bool x,const char *m){if(!x)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<'\n';}
using Pixels=std::vector<uint8_t>;
const Mat4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
void strict_device(EGLDisplay display){
 auto query=reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));
 auto string=reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
 EGLAttrib device=0;check(query&&string&&query(display,EGL_DEVICE_EXT,&device),"Hardware context reports its actual owning EGL device");
 const auto node=string(reinterpret_cast<EGLDeviceEXT>(device),EGL_DRM_RENDER_NODE_FILE_EXT);check(node&&*node,"Actual EGL device declares a DRM render node");
 struct stat st{};check(stat(node,&st)==0&&S_ISCHR(st.st_mode),"Active render node is a real DRM character device");
 const auto sys=std::filesystem::path("/sys/dev/char")/(std::to_string(major(st.st_rdev))+":"+std::to_string(minor(st.st_rdev)))/"device";
 const auto pci=std::filesystem::canonical(sys);auto read=[](const auto &p){std::ifstream f(p);std::string value;f>>value;return value;};
 check(pci.filename()=="0000:03:00.0"&&read(sys/"vendor")=="0x8086"&&read(sys/"device")=="0x56a0","Actual render-node sysfs authority is PCI03 Intel Arc A770");
 bool opened=false;std::string descriptors;
 for(const auto &entry:std::filesystem::directory_iterator("/proc/self/fd")){struct stat fd{};if(stat(entry.path().c_str(),&fd)==0&&S_ISCHR(fd.st_mode)&&fd.st_rdev==st.st_rdev){opened=true;descriptors+=entry.path().filename().string()+",";}}
 check(opened,"Current GL process actually holds the authenticated A770 DRM device open");
 std::cout<<"ACTIVE_DEVICE "<<node<<" rdev="<<major(st.st_rdev)<<":"<<minor(st.st_rdev)<<" pci="<<pci<<" driver="<<std::filesystem::canonical(sys/"driver").filename()<<" fds="<<descriptors<<std::endl;
}
struct Fbo{
 static constexpr int size=256;GLuint f=0,c=0,d=0;
 Fbo(){glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glGenTextures(1,&c);glBindTexture(GL_TEXTURE_2D,c);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c,0);glGenRenderbuffers(1,&d);glBindRenderbuffer(GL_RENDERBUFFER,d);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,size,size);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Independent affine preview FBO is complete");}
 ~Fbo(){glDeleteRenderbuffers(1,&d);glDeleteTextures(1,&c);glDeleteFramebuffers(1,&f);}
 void clear(){glBindFramebuffer(GL_FRAMEBUFFER,f);glViewport(0,0,size,size);glDepthMask(GL_TRUE);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
};
Pixels pixels(){Pixels p(Fbo::size*Fbo::size*4);glReadPixels(0,0,Fbo::size,Fbo::size,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
int delta(const Pixels&a,const Pixels&b){int n=0;for(size_t i=0;i<a.size();++i)n=std::max(n,std::abs(int(a[i])-int(b[i])));return n;}
void image(const std::filesystem::path &p,const Pixels &data){png_image im{};im.version=PNG_IMAGE_VERSION;im.width=im.height=Fbo::size;im.format=PNG_FORMAT_RGBA;if(!png_image_write_to_file(&im,p.c_str(),0,data.data(),0,nullptr))throw std::runtime_error("Cannot preserve independent preview PNG");}
AffineTransform world(bool reflected,int state){auto m=AffineTransform{}.matrix();m[0]=reflected?-1.:1.;m[1]=.125;m[4]=.375;m[5]=.75;m[8]=.25;m[9]=.125;m[10]=.5;m[12]=.125;m[13]=-.125;AffineTransform w(m);if(state==2){auto r=AffineTransform{}.matrix();r[0]=r[5]=0;r[1]=-1;r[4]=1;r[12]=.125;r[13]=.0625;r[14]=-.125;w=AffineTransform(r)*w;}return w;}
// Independent arithmetic, not AffineTransform/model_matrix/placement helpers.
Vec3 expected(Vec3 p,bool reflected,int state,bool canvas){
 Vec3 q{float((reflected?-1.:1.)*p.x+.375*p.y+.25*p.z+.125),float(.125*p.x+.75*p.y+.125*p.z-.125),float(.5*p.z)};
 if(state==2)q={q.y+.125f,-q.x+.0625f,q.z-.125f};
 if(canvas)q={-.75f*q.x+.0625f,.75f*q.y-.0625f,-.75f*q.z+.125f};
 return q;
}
PolygonMesh reference(const PolygonMesh &source,bool reflected,int state,bool canvas,Vec3 move){
 auto encoded=source.encode();std::set<PolygonId> selected;for(auto &c:source.faces()[1].corners)selected.insert(c.vertex);
 for(auto &row:encoded["vertices"]){auto id=row[0].get<PolygonId>();Vec3 p{row[1].get<float>(),row[2].get<float>(),row[3].get<float>()};if(selected.contains(id))p+=move;p=expected(p,reflected,state,canvas);row[1]=p.x;row[2]=p.y;row[3]=p.z;}
 if(reflected)for(auto &face:encoded["faces"])std::reverse(face["corners"].begin(),face["corners"].end());
 return PolygonMesh::decode(encoded);
}
#ifndef DOME_AFFINE_PREVIEW_ONLY
Scene fixture(bool posed,bool reflect){
 auto s=posed?source_color_fixture::model():polygon_test::fixture();
 if(posed){auto d=*s.active().deformation;d.morph_weights={.6f};d.pose[0].translation.x=.1f;s.set_deformation(s.active().id,d);}
 auto g=s.add_group("Actual affine polygon UI parent");s.group(g).affine_edit=std::make_shared<const NativeAffineEdit>(world(reflect,0));s.active().group=g;s.validate();return s;
}
#endif
struct State{
 GLint front,depth,program,vao,array;GLboolean depthmask;bool cull,blend,offset,depth_test,srgb,points;GLfloat factor,units,point_size,line_width;
 State(){glGetIntegerv(GL_FRONT_FACE,&front);glGetIntegerv(GL_DEPTH_FUNC,&depth);glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthmask);cull=glIsEnabled(GL_CULL_FACE);blend=glIsEnabled(GL_BLEND);offset=glIsEnabled(GL_POLYGON_OFFSET_FILL);depth_test=glIsEnabled(GL_DEPTH_TEST);srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);points=glIsEnabled(GL_PROGRAM_POINT_SIZE);glGetFloatv(GL_POLYGON_OFFSET_FACTOR,&factor);glGetFloatv(GL_POLYGON_OFFSET_UNITS,&units);glGetFloatv(GL_POINT_SIZE,&point_size);glGetFloatv(GL_LINE_WIDTH,&line_width);}
 bool same(const State &b)const{return front==b.front&&depth==b.depth&&program==b.program&&vao==b.vao&&array==b.array&&depthmask==b.depthmask&&cull==b.cull&&blend==b.blend&&offset==b.offset&&depth_test==b.depth_test&&srgb==b.srgb&&points==b.points&&factor==b.factor&&units==b.units&&point_size==b.point_size&&line_width==b.line_width;}
};
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum t,GLsizeiptr n,const void*p,GLenum u){if(capture)uploaded+=n;__real_glBufferData(t,n,p,u);}
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferSubData(GLenum t,GLintptr o,GLsizeiptr n,const void*p){if(capture)uploaded+=n;__real_glBufferSubData(t,o,n,p);}
extern "C" void __real_glReadPixels(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*);
extern "C" void __wrap_glReadPixels(GLint x,GLint y,GLsizei w,GLsizei h,GLenum f,GLenum t,void*p){if(capture)++readbacks;__real_glReadPixels(x,y,w,h,f,t,p);}
extern "C" void __real_glGetBufferSubData(GLenum,GLintptr,GLsizeiptr,void*);
extern "C" void __wrap_glGetBufferSubData(GLenum t,GLintptr o,GLsizeiptr n,void*p){if(capture)++readbacks;__real_glGetBufferSubData(t,o,n,p);}
extern "C" void *__real_glMapBuffer(GLenum,GLenum);
extern "C" void *__wrap_glMapBuffer(GLenum t,GLenum a){if(capture)++readbacks;return __real_glMapBuffer(t,a);}
extern "C" void *__real_glMapBufferRange(GLenum,GLintptr,GLsizeiptr,GLbitfield);
extern "C" void *__wrap_glMapBufferRange(GLenum t,GLintptr o,GLsizeiptr n,GLbitfield a){if(capture)++readbacks;return __real_glMapBufferRange(t,o,n,a);}
int main(int argc,char **argv)try{
#ifndef DOME_AFFINE_PREVIEW_ONLY
 if(argc==3&&std::string_view(argv[1])=="--fixtures"){
  std::filesystem::create_directories(argv[2]);for(bool posed:{false,true})for(bool reflected:{false,true})fixture(posed,reflected).save(std::filesystem::path(argv[2])/((posed?"posed-color-":"native-")+std::string(reflected?"reflected":"sheared")+".dsculpt"));return 0;
 }
 if(argc==3&&std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
#endif
 const bool hardware=argc>1&&std::string_view(argv[1])=="--hardware";const auto out=std::filesystem::path(argc>(hardware?2:1)?argv[hardware?2:1]:"polygon-affine-images");std::filesystem::create_directories(out);
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER)),vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<device<<std::endl;
 check(!hardware||(vendor.find("Intel")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos),hardware?"Requested strict Arc A770 renderer identity is actual":"Software offscreen GL context is available (no hardware claim)");
 if(hardware)strict_device(graphics.display);
 Fbo f;PolygonWorkspace workspace(PolygonMesh::cube());workspace.select(workspace.view().topology->faces()[1].id);const auto immutable=workspace.view().surface;
 PolygonRenderer actual,ref,wrong;int maximum=0,wrong_normal=0,wrong_front=0;unsigned pair=0;
 for(bool reflected:{false,true})for(int state=0;state<3;++state){
  if(workspace.view().moving)workspace.end_move(false);
  const Vec3 movement=state?Vec3{.125f,.0625f,0}:Vec3{};if(state){workspace.begin_move();workspace.move_to(movement);}
  const auto w=world(reflected,state);
  auto editor=std::make_shared<EditorState>();editor->generation=1;editor->polygon=std::make_shared<const PolygonView>(workspace.view());editor->polygon_object=1;editor->active_object=1;ObjectInfo object{1,"Affine preview",true,false};object.affine_world=w;object.world_transform_valid=false;editor->objects={object};auto packed=std::make_shared<MeshSnapshot>();packed->generation=1;
  const auto preview=detail::polygon_preview({editor,packed});check(preview.view&&preview.affine_world&&preview.affine_world->matrix()==w.matrix(),"Real preview resolver uses genuine full matrix despite unavailable TRS");
  PolygonWorkspace wanted(reference(*workspace.view().topology,reflected,state,true,movement));wanted.select(workspace.view().topology->faces()[1].id);
  // A wrong raw A*N before VS normalization must be sensitive. Reference
  // geometry remains exact, only deliberately wrong per-face input normals vary.
  auto wrong_view=wanted.view();auto wrong_surface=std::make_shared<PolygonRender>(*wrong_view.surface);
  for(size_t i=0;i<wrong_surface->mesh.normals.size();++i){auto n=workspace.view().surface->mesh.normals.at(i);auto c=workspace.view().display->normal_coefficients.at(i);n+=cross(movement,c);auto origin=expected({},reflected,state,true),q=expected(n,reflected,state,true)-origin;wrong_surface->mesh.normals[i]=q;}
  wrong_view.surface=wrong_surface;
  auto front_view=wanted.view();auto front_surface=std::make_shared<PolygonRender>(*front_view.surface);for(size_t i=0;i<front_surface->mesh.indices.size();i+=3)std::swap(front_surface->mesh.indices[i+1],front_surface->mesh.indices[i+2]);front_view.surface=front_surface;
  for(int view=0;view<3;++view)for(int eye=0;eye<2;++eye){
   Mat4 vp=identity;vp[0]=vp[5]=.75f;vp[10]=-.75f;vp[12]=eye?.015625f:-.015625f;
   if(view==1){vp[0]=.5f;vp[8]=.375f;vp[2]=.375f;vp[10]=-.5f;}
   if(view==2){vp[1]=-.25f;vp[9]=.375f;vp[6]=.25f;vp[5]=.5f;}
   f.clear();glFrontFace(GL_CW);glEnable(GL_CULL_FACE);glEnable(GL_BLEND);glDepthFunc(GL_GREATER);glDepthMask(GL_FALSE);glDisable(GL_DEPTH_TEST);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(3,2);glEnable(GL_FRAMEBUFFER_SRGB);glPointSize(3);glLineWidth(2);const State before;
   // The standalone consumer uses exact AffineTransform multiplication. The
   // actual app GUI separately exercises draw_polygon_preview/model_matrix;
   // pulling the unrelated full SurfaceRenderer TU here would defeat the
   // explicitly bounded lightweight sanitizer consumer.
   auto canvas_matrix=AffineTransform{}.matrix();canvas_matrix[0]=canvas_matrix[10]=-.75;canvas_matrix[5]=.75;canvas_matrix[12]=.0625;canvas_matrix[13]=-.0625;canvas_matrix[14]=.125;
   const auto actual_world=AffineTransform(canvas_matrix)*(*preview.affine_world);
   const auto actual_clip=AffineTransform(vp)*actual_world;
   const auto stats=actual.stats();uploaded=readbacks=0;capture=true;actual.draw(*preview.view,actual_clip.float_matrix(),actual_world.float_matrix(),0xff4020,true);capture=false;
   check(before.same(State{}),"Polygon preview restores caller GL front-face/depth/cull/blend/program/VAO state");
   check(readbacks==0,"Production affine preview performs no geometry/readback query");
   if(pair)check(uploaded==0&&actual.stats().geometry_uploads==stats.geometry_uploads,"Second eye, component motion and held matrix changes reuse exact immutable GPU geometry without uploads");
   const auto got=pixels();f.clear();ref.draw(wanted.view(),vp,identity,0xff4020,true);const auto expected=pixels();const auto error=delta(got,expected);maximum=std::max(maximum,error);
   if(error>1){image(out/("failed-actual-"+std::to_string(pair)+".png"),got);image(out/("failed-reference-"+std::to_string(pair)+".png"),expected);throw std::runtime_error("Affine independent geometry/normal/reflection image error "+std::to_string(error)+" exceeds1");}
   check(error<=1,"Independent baked-world POSITION/Newell normal consumer matches full affine inverse-transpose preview within1");
   size_t coverage=0;for(size_t i=3;i<got.size();i+=4)coverage+=got[i]!=0;check(coverage>1500,"Pixel comparison covers real broad geometry rather than an empty view");
   f.clear();wrong.draw(wrong_view,vp,identity,0xff4020,true);wrong_normal=std::max(wrong_normal,delta(got,pixels()));
   if(reflected){f.clear();wrong.draw(front_view,vp,identity,0xff4020,true);wrong_front=std::max(wrong_front,delta(got,pixels()));}
   if(eye==0&&view==0){image(out/("actual-"+std::to_string(pair)+".png"),got);image(out/("reference-"+std::to_string(pair)+".png"),expected);}
   check(glGetError()==GL_NO_ERROR,"Affine preview/reference GL operations finish without errors");++pair;
  }
  check(workspace.view().surface==immutable,"Full affine field/placement preview leaves original CPU mesh owner immutable");
 }
 check(wrong_normal>4,"Independent wrong A*N convention is observably rejected by unchanged correct image bound");check(wrong_front>4,"Independent omitted reflection/front-face convention is observably rejected");
 std::cout<<checks<<" affine preview checks pairs="<<pair<<" max="<<maximum<<" wrong_normal="<<wrong_normal<<" wrong_front="<<wrong_front<<'\n';return 0;
}catch(const std::exception&e){capture=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
