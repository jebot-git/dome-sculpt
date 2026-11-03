// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_render_main
#include "mtoon-render.cpp"
#undef main
namespace{bool record_uploads=false;size_t uploaded=0;}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void *,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr bytes,const void *data,GLenum usage){if(record_uploads)uploaded+=size_t(bytes);__real_glBufferData(target,bytes,data,usage);}
int main(int argc,char **argv)try{
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<device<<'\n';check(argc<2||std::string(argv[1])!="--hardware"||(vendor=="Intel"&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Strict requested Arc renderer identity");
 Fbo fbo;CursorRenderer cursor;cursor.set_linear_output(true);EditorState state;auto guide=std::make_shared<RigFitView>();guide->step=.1f;guide->role=0;auto source=std::make_shared<DeformationState>();source->skeleton=Skeleton{};source->skeleton->bones={{"Original native root",-1,{{.2f,.15f,0},{},{1,1,1}},.15f},{"Original finger helper",0,{{0,.15f,0},{},{1,1,1}},.1f}};guide->rest_source=source;guide->bone_heads={{.2f,.15f,0},{.2f,.3f,0}};guide->bone_parents={-1,0};guide->bone_lengths={.15f,.1f};guide->world=AffineTransform(std::array<double,16>{-1.2,0,0,0,.3,.8,0,0,0,0,1,0,.6,-.3,0,1});state.rig_fit=guide;
 auto render=[&]{fbo.bind();glViewport(0,0,96,96);glDisable(GL_FRAMEBUFFER_SRGB);glClearColor(0,0,0,0);glDepthMask(GL_TRUE);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);cursor.draw(state,identity);return read();};
 auto count=[&](const auto &bytes,float x,float y,bool selected){int cx=int((x+1)*48),cy=int((y+1)*48),total=0;for(int yy=cy-2;yy<=cy+2;++yy)for(int xx=cx-2;xx<=cx+2;++xx){auto c=at(bytes,xx,yy);if(selected?c[0]>240&&c[1]>150&&c[1]<230:c[1]>240&&c[0]<100)++total;}return total;};
 record_uploads=true;auto first=render();record_uploads=false;
 check(count(first,.405f,-.18f,true)>0,"Independent affine point oracle locates selected original bone after reflection and shear");
 check(count(first,.2f,.15f,true)==0,"Wrong untransformed REST original bone position is visibly rejected");
 check(count(first,.45f,-.06f,false)>0,"Optional guide bone continuation and unselected marker retain independent role colour");
 check(uploaded>0&&uploaded<4096,"Original bone guide uploads only bounded tool lines and extracts no scene geometry");
 check(render()==first,"Second-eye original bone guide image is deterministic with unchanged immutable tool source");
 auto moved=std::make_shared<RigFitView>(*guide);moved->bone_heads[0].x+=.2f;state.rig_fit=moved;auto after=render();check(after!=first&&count(after,.165f,-.18f,true)>0,"Original bone edit changes the guide at independently computed full-affine location");
 state.rig_fit.reset();auto absent=render();check(std::all_of(absent.begin(),absent.end(),[](auto x){return x==0;}),"Cancelling fitting removes all guide pixels without altering a model");
 check(glGetError()==GL_NO_ERROR,"Rig-fitting guide preserves valid OpenGL state");std::cout<<checks<<" humanoid fitting GPU guide checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
