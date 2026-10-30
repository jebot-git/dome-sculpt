// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-browser.hpp"
#include "text-utf8.hpp"
#include <algorithm>
#include <stdexcept>
namespace dome {
void ImportSettings::validate() const {
  choices.validate();
  if(paint_resolution<64 || paint_resolution>2048 || (paint_resolution&(paint_resolution-1)))
    throw std::invalid_argument("Paint resolution must be a power of two from 64 to 2048");
}
const char *import_format_label(ImportSourceFormat v){static const char *names[]={"From filename","GLB","glTF","OBJ","FBX"};return int(v)>=0&&int(v)<5?names[int(v)]:"Invalid format";}
const char *import_units_label(ImportUnits v){
  static const char *names[]={"From file","Metres","Centimetres","Millimetres","Custom metres per unit"};
  return int(v)>=0&&int(v)<5?names[int(v)]:"Invalid units";
}
const char *import_axes_label(ImportAxes v){
  static const char *names[]={"From file","Y-up","Z-up"};
  return int(v)>=0&&int(v)<3?names[int(v)]:"Invalid axes";
}
const char *import_normals_label(ImportNormals v){
  static const char *names[]={"Preserve","Generate flat","Generate smooth"};
  return int(v)>=0&&int(v)<3?names[int(v)]:"Invalid normals";
}
ImportView scan_import_directory(const std::filesystem::path &path,const std::function<bool()> &cancelled){
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Import browsing cancelled");};
  stop();ImportView result;result.directory=std::filesystem::canonical(path);
  size_t visits=0,bytes=0,unreadable=0;
  for(const auto &entry:std::filesystem::directory_iterator(result.directory)){
    stop();if(++visits>16384)throw std::length_error("Import folder contains too many entries; choose a smaller folder");
    auto name=entry.path().filename().string();
    if(!text::valid_utf8(name)){++unreadable;continue;}
    std::error_code error;auto status=entry.status(error);if(error){++unreadable;continue;}
    bool directory=std::filesystem::is_directory(status);
    if(!directory){
      if(!std::filesystem::is_regular_file(status))continue;
      auto extension=entry.path().extension().string();for(auto &c:extension)c=char(std::tolower(static_cast<unsigned char>(c)));
      if(extension!=".obj"&&extension!=".fbx"&&extension!=".gltf"&&extension!=".glb"&&extension!=".vrm")continue;
    }
    if(result.entries.size()>=512 || name.size()>256*1024-bytes)
      throw std::length_error("Import folder listing exceeds its bound; choose a smaller folder");
    bytes+=name.size();result.entries.push_back({std::move(name),directory});
  }
  if(unreadable)result.error="Skipped "+std::to_string(unreadable)+" unreadable or invalid-name entries";
  stop();std::sort(result.entries.begin(),result.entries.end(),[](const auto&a,const auto&b){return a.directory!=b.directory?a.directory>b.directory:a.name<b.name;});
  return result;
}
} // namespace dome
