// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <EGL/eglext.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <fstream>
#include <iterator>
#include <string_view>

namespace sustained::device {
using nlohmann::json;
inline void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
inline std::string text(const std::filesystem::path &p){std::ifstream f(p);require(bool(f),"Cannot read actual device evidence");return {std::istreambuf_iterator<char>(f),{}};}
inline std::string gl_string(GLenum name){const auto *value=glGetString(name);require(value,"No actual GL identity string");return reinterpret_cast<const char*>(value);}
inline bool extension(std::string_view name){GLint count=0;glGetIntegerv(GL_NUM_EXTENSIONS,&count);for(GLint i=0;i<count;++i){const auto *value=glGetStringi(GL_EXTENSIONS,GLuint(i));if(value&&name==reinterpret_cast<const char*>(value))return true;}return false;}
inline json authenticate(std::string_view mode){
  const auto renderer=gl_string(GL_RENDERER),vendor=gl_string(GL_VENDOR),version=gl_string(GL_VERSION);
  json result={{"requested",mode},{"renderer",renderer},{"vendor",vendor},{"version",version}};
  const bool software=renderer.find("llvmpipe")!=std::string::npos||renderer.find("softpipe")!=std::string::npos;
  if(mode=="software"){
    require(software&&vendor.find("Mesa")!=std::string::npos,"Actual context is not requested Mesa software GL");
    result["authenticated"]=true;result["drm_node_scope"]="software GL, no hardware claim";return result;
  }
  require(mode=="arc","Unknown requested render device");
  require(!software&&vendor.find("Intel")!=std::string::npos&&renderer.find("Arc")!=std::string::npos&&renderer.find("A770")!=std::string::npos,"Actual context is not strict Intel Arc A770");
  const auto display=eglGetCurrentDisplay();require(display!=EGL_NO_DISPLAY&&eglGetCurrentContext()!=EGL_NO_CONTEXT,"No actual current EGL context");
  auto query_display=reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));
  auto query_device=reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
  EGLAttrib device=0;require(query_display&&query_device&&query_display(display,EGL_DEVICE_EXT,&device)==EGL_TRUE&&device,"Current EGL display device witness unavailable");
  const char *node=query_device(reinterpret_cast<EGLDeviceEXT>(device),EGL_DRM_RENDER_NODE_FILE_EXT);require(node&&*node,"Current EGL device has no actual render-node witness");
  const auto actual=std::filesystem::canonical(node),expected=std::filesystem::canonical("/dev/dri/by-path/pci-0000:03:00.0-render");
  struct stat actual_stat{},expected_stat{};
  require(stat(actual.c_str(),&actual_stat)==0&&stat(expected.c_str(),&expected_stat)==0&&S_ISCHR(actual_stat.st_mode)&&actual==expected&&actual_stat.st_rdev==expected_stat.st_rdev,"Actual EGL device is not the declared PCI03 Arc render node");
  const auto sysfs=std::filesystem::path("/sys/dev/char")/(std::to_string(major(actual_stat.st_rdev))+":"+std::to_string(minor(actual_stat.st_rdev)));
  const auto pci=std::filesystem::canonical(sysfs/"device"),driver=std::filesystem::canonical(pci/"driver");
  require(pci.filename()=="0000:03:00.0"&&std::stoul(text(pci/"vendor"),nullptr,16)==0x8086&&std::stoul(text(pci/"device"),nullptr,16)==0x56a0&&driver.filename()=="i915","Actual render node does not match PCI03/8086:56a0/i915");
  json fds=json::array();
  for(const auto &entry:std::filesystem::directory_iterator("/proc/self/fd")){
    const auto name=entry.path().filename().string();const int fd=std::stoi(name);struct stat status{};
    if(fstat(fd,&status)==0&&S_ISCHR(status.st_mode)&&status.st_rdev==actual_stat.st_rdev){std::error_code ec;const auto target=std::filesystem::read_symlink(entry.path(),ec);fds.push_back({{"fd",fd},{"target",ec?"<closed during observation>":target.string()}});}
  }
  require(!fds.empty(),"Current process has no live FD on the authenticated Arc render node");
  result["authenticated"]=true;result["render_node"]=actual.string();result["device_major"]=major(actual_stat.st_rdev);result["device_minor"]=minor(actual_stat.st_rdev);result["pci"]=pci.string();result["vendor_id"]=text(pci/"vendor");result["device_id"]=text(pci/"device");result["driver"]=driver.string();result["open_fds"]=std::move(fds);return result;
}
}
