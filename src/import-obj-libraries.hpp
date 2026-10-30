// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import-source.hpp"
#include <map>
#include <set>
namespace dome::detail {
struct ObjLibraries {
  std::vector<uint8_t> canonical_mtl;
  std::map<std::string,std::string> material_directories;
};
// Native OBJ has a shared material-name namespace. Capture all explicitly
// listed libraries; the parser's memory-only adapter consumes their same bytes.
// Relative texture paths are resolved from their originating MTL directory.
inline ObjLibraries capture_obj_libraries(ImportSourceCapture &capture,const ImportLimits &limits) {
  auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("OBJ library capture cancelled");};
  auto lines=[&](std::span<const uint8_t> bytes,auto callback){
    std::string logical;size_t count=0;
    for(size_t at=0;at<bytes.size();){
      stop();size_t end=at;while(end<bytes.size()&&bytes[end]!='\n')++end;
      auto line=std::string_view(reinterpret_cast<const char*>(bytes.data()+at),end-at);
      if(!line.empty()&&line.back()=='\r')line.remove_suffix(1);
      bool continuation=!line.empty()&&line.back()=='\\';if(continuation)line.remove_suffix(1);
      if(line.size()>1024*1024-std::min(logical.size(),size_t(1024*1024)))throw std::length_error("OBJ/MTL line exceeds bounded parser input");
      logical.append(line);if(continuation)logical+=' ';
      else {if(++count>limits.max_metadata_visits)throw std::length_error("OBJ/MTL metadata work limit exceeded");callback(std::string_view(logical));logical.clear();}
      at=end<bytes.size()?end+1:end;
    }
    if(!logical.empty())throw std::runtime_error("Unterminated OBJ/MTL continuation");
  };
  auto tokens=[](std::string_view line){
    std::vector<std::string> result;std::string token;char quote=0;bool active=false;
    for(size_t i=0;i<line.size();++i){char c=line[i];
      if(c=='\\'&&i+1<line.size()&&(line[i+1]==' '||line[i+1]=='\t'||line[i+1]=='"'||line[i+1]=='\''||line[i+1]=='#')){token+=line[++i];active=true;}
      else if(quote){if(c==quote)quote=0;else token+=c;active=true;}
      else if(c=='"'||c=='\''){quote=c;active=true;}
      else if(c=='#')break;
      else if(c==' '||c=='\t'){if(active){result.push_back(std::move(token));token.clear();active=false;}}
      else {token+=c;active=true;}
    }
    if(quote)throw std::runtime_error("Unterminated OBJ/MTL quoted name");
    if(active)result.push_back(std::move(token));
    return result;
  };
  ObjLibraries out;std::set<std::string> libraries;
  lines(*capture.source().root.bytes,[&](std::string_view line){
    auto t=tokens(line);if(t.empty()||t[0]!="mtllib")return;
    if(t.size()<2)throw std::runtime_error("OBJ material library has no filename");
    for(size_t i=1;i<t.size();++i){
      std::replace(t[i].begin(),t[i].end(),'\\','/');auto name=std::filesystem::path(t[i]).lexically_normal().generic_string();
      if(!libraries.insert(name).second)continue;
      auto bytes=capture.resource(name);
      if(bytes->size()>=16*1024*1024-std::min(out.canonical_mtl.size(),size_t(16*1024*1024)))
        throw std::length_error("OBJ material library aggregate exceeds 16MiB");
      lines(*bytes,[&](std::string_view raw){auto m=tokens(raw);if(m.empty()||m[0]!="newmtl")return;
        if(m.size()<2)throw std::runtime_error("OBJ material has no name");
        std::string material=m[1];for(size_t j=2;j<m.size();++j)material+=' '+m[j];
        if(!out.material_directories.emplace(material,std::filesystem::path(name).parent_path().generic_string()).second)
          throw std::runtime_error("Repeated OBJ material name requires explicit material authority: "+material);
      });
      out.canonical_mtl.insert(out.canonical_mtl.end(),bytes->begin(),bytes->end());out.canonical_mtl.push_back('\n');
    }
  });return out;
}
} // namespace dome::detail
