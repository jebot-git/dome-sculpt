// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_render_main
#include "mtoon-render.cpp"
#undef main
namespace {bool recording=false;size_t geometry_bytes=0,image_uploads=0;}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage){if(recording&&(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER))geometry_bytes+=size;__real_glBufferData(target,size,data,usage);}
extern "C" void __real_glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
extern "C" void __wrap_glTexImage2D(GLenum a,GLint b,GLint c,GLsizei d,GLsizei e,GLint f,GLenum g,GLenum h,const void *i){if(recording)++image_uploads;__real_glTexImage2D(a,b,c,d,e,f,g,h,i);}
int main(int argc,char **argv)try{
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<device<<'\n';check(argc<2||std::string(argv[1])!="--hardware"||(vendor=="Intel"&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Strict requested Arc renderer identity");
 Fbo fbo;CursorRenderer cursor;cursor.set_linear_output(true);EditorState state;auto view=std::make_shared<ProjectionView>();auto tool=std::make_shared<ProjectionToolImage>();tool->pixels=image({255,0,0,255});tool->name="Red";view->image=tool;view->settings.camera.pose.position={0,0,1};view->settings.camera.width=view->settings.camera.height=1;view->settings.preview_depth=1;state.projection=view;
 auto render=[&]{fbo.bind();glViewport(0,0,96,96);glDisable(GL_FRAMEBUFFER_SRGB);glClearColor(0,0,0,0);glDepthMask(GL_TRUE);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);cursor.draw(state,identity);return read();};
 auto first=render();check(close(at(first),{115,0,0,115},2),"Independent constant-image preview blend oracle");check(at(first,10,10)[0]==0,"Projector plane has bounded metric extent");
 recording=true;geometry_bytes=image_uploads=0;auto second=render();recording=false;check(first==second&&geometry_bytes==0&&image_uploads==0,"Second eye reuses immutable image and constant geometry without uploads");
 auto moved=std::make_shared<ProjectionView>(*view);moved->settings.camera.pose.position.x=.3f;state.projection=moved;recording=true;geometry_bytes=image_uploads=0;auto shifted=render();recording=false;check(shifted!=first&&geometry_bytes==0&&image_uploads==0,"Projector placement changes only GPU uniforms");
 auto stencil=std::make_shared<ProjectionView>(*moved);stencil->settings.image.usage=ProjectionUsage::Stencil;stencil->settings.image.scalar=ProjectionScalar::Red;state.projection=stencil;auto gray=render();check(close(at(gray,60,48),{115,115,115,115},2),"Stencil preview uses selected scalar channel independently");
 auto disabled=std::make_shared<ProjectionView>(*view);disabled->settings.preview=false;state.projection=disabled;auto empty=render();check(at(empty)==std::array<int,4>{0,0,0,0},"Preview switch removes projector without affecting source geometry");

 auto gray_tool=std::make_shared<ProjectionToolImage>();gray_tool->pixels=image({128,128,128,255});auto gamma=std::make_shared<ProjectionView>(*view);gamma->image=gray_tool;state.projection=gamma;auto srgb=render();check(close(at(srgb),{25,25,25,115},2),"Production linear preview independently decodes sRGB image before blending");
 auto linear=std::make_shared<ProjectionView>(*gamma);linear->settings.image.color_space=ProjectionColorSpace::Linear;state.projection=linear;recording=true;geometry_bytes=image_uploads=0;auto data=render();recording=false;check(close(at(data),{58,58,58,115},2)&&geometry_bytes==0&&image_uploads==0,"Explicit linear preview changes interpretation via uniforms, with no pixel reupload");
 auto edge_tool=std::make_shared<ProjectionToolImage>();edge_tool->pixels=std::make_shared<const Image>(Image{2,1,{255,0,0,255,0,0,255,0}});auto edge=std::make_shared<ProjectionView>(*view);edge->image=edge_tool;state.projection=edge;auto filtered=at(render());check(filtered[0]>40&&filtered[0]<75&&filtered[2]==0,"GPU preview filters premultiplied linear texels without transparent-blue fringes");
 check(glGetError()==GL_NO_ERROR,"Projector preview leaves a valid GPU state");std::cout<<checks<<" GPU projection preview checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
