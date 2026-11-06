// Test-only Release link seam: forwards the unchanged renderer query, then
// authenticates the actual current EGL/GL device before any quality assertions.
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <cstdlib>
namespace {
std::string read(const std::filesystem::path &path){std::ifstream f(path);return {std::istreambuf_iterator<char>(f),{}};}
void require(bool ok,const char *reason){if(!ok){std::cerr<<"STRICT_MARKER_REFUSAL "<<reason<<std::endl;throw std::runtime_error(reason);}}
}
extern "C" const GLubyte *__real_glGetString(GLenum);
extern "C" const GLubyte *__wrap_glGetString(GLenum name){
 const auto *result=__real_glGetString(name);
 if(name!=GL_RENDERER)return result;
 static bool authenticated=false;if(authenticated)return result;
 const auto *v=__real_glGetString(GL_VENDOR);const auto *version=__real_glGetString(GL_VERSION);
 const std::string renderer=result?reinterpret_cast<const char*>(result):"<null>",vendor=v?reinterpret_cast<const char*>(v):"<null>";
 std::cerr<<"STRICT_MARKER_ACTUAL vendor="<<vendor<<" renderer="<<renderer<<" version="<<(version?reinterpret_cast<const char*>(version):"<null>")<<std::endl;
 require(vendor.find("Intel")!=std::string::npos&&renderer.find("Arc")!=std::string::npos&&renderer.find("A770")!=std::string::npos&&renderer.find("llvmpipe")==std::string::npos&&renderer.find("softpipe")==std::string::npos,
         "Actual renderer is not strict Intel Arc A770");
 const auto display=eglGetCurrentDisplay();require(display!=EGL_NO_DISPLAY&&eglGetCurrentContext()!=EGL_NO_CONTEXT,"No actual current EGL context");
 auto query_display=reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));
 auto query_device=reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
 EGLAttrib device=0;require(query_display&&query_device&&query_display(display,EGL_DEVICE_EXT,&device)==EGL_TRUE&&device,"Actual EGL display device query is unavailable");
 const char *node=query_device(reinterpret_cast<EGLDeviceEXT>(device),EGL_DRM_RENDER_NODE_FILE_EXT);
 require(node&&*node,"Actual EGL device has no DRM render node");
 const auto actual=std::filesystem::canonical(node),expected=std::filesystem::canonical("/dev/dri/by-path/pci-0000:03:00.0-render");
 struct stat actual_stat{},expected_stat{};require(stat(actual.c_str(),&actual_stat)==0&&stat(expected.c_str(),&expected_stat)==0&&S_ISCHR(actual_stat.st_mode)&&actual_stat.st_rdev==expected_stat.st_rdev&&actual==expected,"Actual EGL render node is not PCI03 renderD129");
 const auto pci=std::filesystem::canonical(std::filesystem::path("/sys/dev/char")/(std::to_string(major(actual_stat.st_rdev))+":"+std::to_string(minor(actual_stat.st_rdev)))/"device");
 require(pci.filename()=="0000:03:00.0"&&std::stoul(read(pci/"vendor"),nullptr,16)==0x8086&&std::stoul(read(pci/"device"),nullptr,16)==0x56a0,"Actual render node PCI vendor/device is not8086:56a0 at03:00.0");
 std::vector<int> fds;
 for(const auto &entry:std::filesystem::directory_iterator("/proc/self/fd")){
   const int fd=std::stoi(entry.path().filename().string());struct stat st{};
   if(fstat(fd,&st)==0&&S_ISCHR(st.st_mode)&&st.st_rdev==actual_stat.st_rdev){fds.push_back(fd);std::cerr<<"STRICT_MARKER_OPEN_FD "<<fd<<" target="<<std::filesystem::read_symlink(entry.path())<<std::endl;}
 }
 require(!fds.empty(),"Actual context has no open FD on selected PCI03 render node");
 std::cerr<<"STRICT_MARKER_DEVICE node="<<actual<<" dev="<<major(actual_stat.st_rdev)<<':'<<minor(actual_stat.st_rdev)<<" pci="<<pci<<" vendor="<<read(pci/"vendor")<<" device="<<read(pci/"device")<<" kernel_driver="<<std::filesystem::canonical(pci/"driver")<<std::endl;
 if(auto directory=std::getenv("DOME_MARKER_DEVICE_REPORT")){
   const std::filesystem::path output(directory);std::filesystem::create_directories(output);
   std::ofstream(output/"context-maps.txt")<<read("/proc/self/maps");
   std::ofstream report(output/"device.txt");report<<"vendor="<<vendor<<"\nrenderer="<<renderer<<"\nrender_node="<<actual.string()<<"\npci="<<pci.string()<<"\nvendor_id="<<read(pci/"vendor")<<"device_id="<<read(pci/"device");
   for(int fd:fds)report<<"fd="<<fd<<'\n';
   require(bool(report),"Cannot record actual strict device evidence");
 }
 authenticated=true;return result;
}
