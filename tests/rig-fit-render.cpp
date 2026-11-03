// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_render_main
#include "mtoon-render.cpp"
#undef main
namespace{bool record_uploads=false;size_t uploaded=0;}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void *,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr bytes,const void *data,GLenum usage){if(record_uploads)uploaded+=size_t(bytes);__real_glBufferData(target,bytes,data,usage);}
int main(int argc,char **argv)try{
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<device<<'\n';check(argc<2||std::string(argv[1])!="--hardware"||(vendor=="Intel"&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Strict requested Arc renderer identity");
 Fbo fbo;CursorRenderer cursor;cursor.set_linear_output(true);EditorState state;auto guide=std::make_shared<RigFitView>();guide->step=.1f;guide->role=0;guide->landmarks.points={{"hips",{.2f,.15f,0}},{"spine",{.2f,.3f,0}}};guide->world=AffineTransform(std::array<double,16>{-1.2,0,0,0,.3,.8,0,0,0,0,1,0,.6,-.3,0,1});state.rig_fit=guide;
 auto render=[&]{fbo.bind();glViewport(0,0,96,96);glDisable(GL_FRAMEBUFFER_SRGB);glClearColor(0,0,0,0);glDepthMask(GL_TRUE);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);cursor.draw(state,identity);return read();};
 auto count=[&](const auto &bytes,float x,float y,bool selected){int cx=int((x+1)*48),cy=int((y+1)*48),total=0;for(int yy=cy-2;yy<=cy+2;++yy)for(int xx=cx-2;xx<=cx+2;++xx){auto c=at(bytes,xx,yy);if(selected?c[0]>240&&c[1]>150&&c[1]<230:c[1]>240&&c[0]<100)++total;}return total;};
 record_uploads=true;auto first=render();record_uploads=false;
 check(count(first,.405f,-.18f,true)>0,"Independent affine point oracle locates selected landmark after reflection and shear");
 check(count(first,.2f,.15f,true)==0,"Wrong untransformed REST landmark position is visibly rejected");
 check(count(first,.45f,-.06f,false)>0,"Optional guide bone continuation and unselected marker retain independent role colour");
 check(uploaded>0&&uploaded<4096,"Landmark guide uploads only bounded tool lines and extracts no scene geometry");
 check(render()==first,"Second-eye landmark guide image is deterministic with unchanged immutable tool source");
 auto selected_spine=std::make_shared<RigFitView>(*guide);const auto &roles=human_roles();auto spine=std::find_if(roles.begin(),roles.end(),[](const auto &r){return r.name=="spine";});check(spine!=roles.end(),"Selection change uses the actual required spine role");selected_spine->role=size_t(spine-roles.begin());state.rig_fit=selected_spine;auto selected=render();
 check(count(selected,.45f,-.06f,true)>0&&count(selected,.405f,-.18f,false)>0,"Changing the required landmark moves its highlight while the prior landmark retains its unselected colour");
 auto existing=std::make_shared<RigFitView>(*selected_spine);auto deformation=std::make_shared<DeformationState>(quad());deformation->skeleton=Skeleton{{{"Root (required)",-1,{},.15f},{"Editable child",0,{},.2f}}};deformation->validate(quad());existing->rest_source=deformation;existing->bone_heads={{.2f,.15f,0},{.2f,.3f,0}};existing->bone_parents={-1,0};existing->bone_lengths={.15f,.2f};existing->role=1;state.rig_fit=existing;
 check(render()==selected,"Existing-rig joint selection retains the same marker geometry and colours independently of bone display names");
 state.rig_fit=guide;check(render()==first,"Returning to the original immutable landmark view restores its exact guide pixels");
 auto moved=std::make_shared<RigFitView>(*guide);moved->landmarks.points["hips"].x+=.2f;state.rig_fit=moved;auto after=render();check(after!=first&&count(after,.165f,-.18f,true)>0,"Landmark edit changes the guide at independently computed full-affine location");
 state.rig_fit.reset();auto absent=render();check(std::all_of(absent.begin(),absent.end(),[](auto x){return x==0;}),"Cancelling fitting removes all guide pixels without altering a model");
 check(glGetError()==GL_NO_ERROR,"Rig-fitting guide preserves valid OpenGL state");std::cout<<checks<<" humanoid fitting GPU guide checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
