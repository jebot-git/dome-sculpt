// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-source.hpp"
#include "text-utf8.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/openat2.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <utility>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace dome {
namespace {
constexpr size_t file_limit=128*1024*1024, total_limit=256*1024*1024;
void stop(const ImportLimits &l) {
  if(l.cancelled && l.cancelled()) throw std::runtime_error("Source capture cancelled");
}
void valid_limits(const ImportLimits &l) {
  if(!l.max_file_bytes || l.max_file_bytes>file_limit ||
      !l.max_resource_bytes || l.max_resource_bytes>total_limit || !l.max_metadata_visits)
    throw std::invalid_argument("Invalid source capture resource limits");
}
struct Fd {
  int value=-1;
  explicit Fd(int v=-1):value(v){}
  ~Fd(){if(value>=0)close(value);}
  Fd(const Fd&)=delete;
};
std::string normalized(std::string_view name) {
  if(name.empty() || name.size()>4096 || !text::valid_utf8(name) ||
      name.find('\0')!=name.npos || name.find('\\')!=name.npos ||
      name.find(':')!=name.npos || name.front()=='/')
    throw std::invalid_argument("Unsupported source resource path");
  std::filesystem::path p(name);
  p=p.lexically_normal();
  if(p.begin()!=p.end() && *p.begin()=="..")
    throw std::invalid_argument("Source resource escapes its asset directory");
  auto result=p.generic_string();
  if(result.empty() || result=="." || result.back()=='/')
    throw std::invalid_argument("Source resource path must name a file");
  return result;
}
std::string uri_path(std::string_view uri) {
  std::string out;
  auto hex=[](char c)->int {return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
  if(uri.find('?')!=uri.npos || uri.find('#')!=uri.npos)
    throw std::invalid_argument("Unsupported source resource URI suffix");
  for(size_t i=0;i<uri.size();++i) {
    if(uri[i]=='%') {
      if(i+2>=uri.size() || hex(uri[i+1])<0 || hex(uri[i+2])<0)
        throw std::invalid_argument("Invalid source resource URI escape");
      out+=char(hex(uri[i+1])*16+hex(uri[i+2])); i+=2;
    } else out+=uri[i];
  }
  return normalized(out);
}
int beneath(int root,const std::string &name) {
  open_how how{};how.flags=O_RDONLY|O_CLOEXEC|O_NONBLOCK;
  how.resolve=RESOLVE_BENEATH|RESOLVE_NO_MAGICLINKS;
  int fd=int(syscall(SYS_openat2,root,name.c_str(),&how,sizeof(how)));
  if(fd>=0 || errno!=ENOSYS) return fd;
  // Older kernels: deliberately disallow all symlinks instead of weakening
  // confinement through a check-then-open path race.
  Fd current(fcntl(root,F_DUPFD_CLOEXEC,0));
  if(current.value<0)return -1;
  std::filesystem::path p(name);
  for(auto i=p.begin();i!=p.end();++i) {
    auto next=i;++next;
    int flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK;
    if(next!=p.end())flags|=O_DIRECTORY;
    int fd2=openat(current.value,i->c_str(),flags);
    const int saved_errno=errno;
    close(current.value);current.value=fd2;
    if(fd2<0){errno=saved_errno;return -1;}
  }
  return std::exchange(current.value,-1);
}
std::shared_ptr<const std::vector<uint8_t>> read(int root,const std::string &name,
    const ImportLimits &l,size_t remaining) {
  stop(l);Fd fd(beneath(root,name));
  if(fd.value<0)throw std::runtime_error("Cannot capture source resource '"+name+"': "+std::strerror(errno));
  struct stat before{},after{};
  if(fstat(fd.value,&before) || !S_ISREG(before.st_mode) || before.st_size<0)
    throw std::runtime_error("Source resource must be a regular file: "+name);
  if(uint64_t(before.st_size)>std::min(l.max_file_bytes,remaining))
    throw std::length_error("Source resource byte budget exceeded: "+name);
  auto data=std::make_shared<std::vector<uint8_t>>(size_t(before.st_size));
  for(size_t at=0;at<data->size();) {
    stop(l);auto n=::read(fd.value,data->data()+at,std::min(size_t(65536),data->size()-at));
    if(n<0 && errno==EINTR)continue;
    if(n<=0)throw std::runtime_error("Source changed or could not be completely captured: "+name);
    at+=size_t(n);
  }
  stop(l);
  if(fstat(fd.value,&after) || before.st_dev!=after.st_dev || before.st_ino!=after.st_ino ||
      before.st_size!=after.st_size || before.st_mtim.tv_sec!=after.st_mtim.tv_sec ||
      before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec || before.st_ctim.tv_sec!=after.st_ctim.tv_sec ||
      before.st_ctim.tv_nsec!=after.st_ctim.tv_nsec)
    throw std::runtime_error("Source changed during capture; retry import: "+name);
  return data;
}
ImportSourceFormat format(std::string extension) {
  for(char &c:extension)c=char(std::tolower(static_cast<unsigned char>(c)));
  if(extension==".glb" || extension==".vrm")return ImportSourceFormat::Glb;
  if(extension==".gltf")return ImportSourceFormat::Gltf;
  if(extension==".obj")return ImportSourceFormat::Obj;
  if(extension==".fbx")return ImportSourceFormat::Fbx;
  throw std::invalid_argument("Choose a standard GLB, glTF, OBJ or FBX model");
}
uint32_t word(std::span<const uint8_t>b,size_t at) {
  if(at>b.size() || b.size()-at<4)throw std::runtime_error("Truncated GLB source");
  return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;
}
void append_word(std::vector<uint8_t>&out,uint32_t v) {
  for(int i=0;i<4;++i)out.push_back(uint8_t(v>>(i*8)));
}
std::vector<uint8_t> data_uri(std::string_view uri,const ImportLimits &l) {
  auto comma=uri.find(',');
  if(!uri.starts_with("data:") || comma==uri.npos || comma<12 || uri.substr(comma-7,7)!=";base64")
    throw std::invalid_argument("Only base64 data URIs are supported");
  auto text=uri.substr(comma+1);
  if(text.size()%4 || text.size()/4*3>l.max_resource_bytes+2)
    throw std::length_error("Invalid or excessive source data URI");
  auto digit=[](char c){return c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='+'?62:c=='/'?63:-1;};
  std::vector<uint8_t> out;out.reserve(text.size()/4*3);
  for(size_t i=0;i<text.size();i+=4) {
    if((i&65535)==0)stop(l);
    uint32_t v=0;int pad=0;
    for(int k=0;k<4;++k) {
      if(text[i+k]=='=') {
        if(k<2 || i+4!=text.size())throw std::invalid_argument("Invalid data URI padding");
        ++pad;v<<=6;
      } else {
        int d=digit(text[i+k]);
        if(pad || d<0)throw std::invalid_argument("Invalid data URI encoding");
        v=v<<6|uint32_t(d);
      }
    }
    if(pad>2)throw std::invalid_argument("Invalid data URI padding");
    out.push_back(uint8_t(v>>16));if(pad<2)out.push_back(uint8_t(v>>8));if(!pad)out.push_back(uint8_t(v));
  }
  return out;
}
size_t integer(const nlohmann::json &j) {
  if(!j.is_number_integer() || (j.is_number_integer()&&!j.is_number_unsigned()&&j.get<int64_t>()<0) || j.get<uint64_t>()>UINT32_MAX)
    throw std::invalid_argument("Invalid source buffer index or offset");
  return j.get<size_t>();
}
} // namespace
struct ImportSourceCapture::State {
  Fd directory;
  ImportLimits limits;
  ImportSourceReceipt receipt;
  std::map<std::string,size_t> names;
  size_t total=0;
  bool finished=false;
  State()=default;
};
size_t ImportSourceReceipt::stored_bytes() const {
  size_t result=root.bytes?root.bytes->size():0;
  for(const auto &r:resources) {
    if(!r.bytes || r.bytes->size()>SIZE_MAX-result)throw std::length_error("Invalid source receipt byte count");
    result+=r.bytes->size();
  }
  return result;
}
void ImportSourceReceipt::validate(const ImportLimits &limits) const {
  valid_limits(limits);options.validate();stop(limits);
  if(format<ImportSourceFormat::Glb || format>ImportSourceFormat::Fbx || resources.size()>511 ||
      adapter.empty() || adapter.size()>256 || !text::valid_utf8(adapter) || adapter.find('\0')!=adapter.npos)
    throw std::invalid_argument("Invalid captured source receipt");
  std::set<std::string> names;
  auto file=[&](const ImportSourceFile &r) {
    stop(limits);
    if(normalized(r.name)!=r.name || !r.bytes || r.bytes->size()>limits.max_file_bytes || !names.insert(r.name).second)
      throw std::invalid_argument("Invalid captured source file identity");
  };
  file(root);for(const auto &r:resources)file(r);
  if(stored_bytes()>limits.max_resource_bytes)throw std::length_error("Captured source aggregate byte budget exceeded");
}
nlohmann::json ImportSourceReceipt::encode() const {
  validate();auto files=nlohmann::json::array();
  for(const auto &r:resources)files.push_back({{"name",r.name},{"bytes",nlohmann::json::binary(*r.bytes)}});
  return {{"version",1},{"format",int(format)},{"options",options.encode()},
    {"adapter",adapter},{"root",{{"name",root.name},{"bytes",nlohmann::json::binary(*root.bytes)}}},{"resources",std::move(files)}};
}
size_t ImportSourceReceipt::encoded_bytes(const nlohmann::json &j,const ImportLimits &l) {
  valid_limits(l);stop(l);
  if(!j.is_object() || j.at("version")!=1 || !j.at("format").is_number_integer() ||
      j.at("format").get<int64_t>()<1 || j.at("format").get<uint64_t>()>4 ||
      !j.at("resources").is_array() || j.at("resources").size()>511)
    throw std::invalid_argument("Unsupported captured source receipt");
  size_t total=0;std::set<std::string> names;
  auto preflight=[&](const nlohmann::json &v) {
    stop(l);auto name=v.at("name").get<std::string>();const auto &bytes=v.at("bytes");
    if(normalized(name)!=name || !names.insert(name).second || !bytes.is_binary() ||
        bytes.get_binary().size()>l.max_file_bytes || bytes.get_binary().size()>l.max_resource_bytes-total)
      throw std::length_error("Invalid captured source receipt payload");
    total+=bytes.get_binary().size();
  };
  preflight(j.at("root"));for(const auto &v:j.at("resources"))preflight(v);
  (void)ImportOptions::decode(j.at("options"));
  auto adapter=j.at("adapter").get<std::string>();
  if(adapter.empty()||adapter.size()>256||!text::valid_utf8(adapter)||adapter.find('\0')!=adapter.npos)throw std::invalid_argument("Invalid captured source adapter identity");
  return total;
}
std::shared_ptr<const ImportSourceReceipt> ImportSourceReceipt::decode(const nlohmann::json &j,const ImportLimits &l) {
  (void)encoded_bytes(j,l);
  auto out=std::make_shared<ImportSourceReceipt>();out->format=ImportSourceFormat(j.at("format").get<int>());
  out->options=ImportOptions::decode(j.at("options"));out->adapter=j.at("adapter").get<std::string>();
  auto file=[](const nlohmann::json &v) {const auto &b=v.at("bytes").get_binary();return ImportSourceFile{v.at("name").get<std::string>(),std::make_shared<const std::vector<uint8_t>>(b.begin(),b.end())};};
  out->root=file(j.at("root"));for(const auto &v:j.at("resources")){stop(l);out->resources.push_back(file(v));}
  out->validate(l);return out;
}
ImportSourceCapture::ImportSourceCapture(const std::filesystem::path &path,ImportOptions options,ImportLimits limits) {
  options.validate();valid_limits(limits);stop(limits);
  auto parent=path.parent_path();if(parent.empty())parent=".";
  Fd directory(open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
  if(directory.value<0)throw std::runtime_error("Cannot open model source directory");
  // Keep the open descriptor in its stack owner until allocating State has
  // succeeded. Evaluating dup() as a make_unique argument leaked on bad_alloc.
  state_=std::make_unique<State>();
  state_->directory.value=std::exchange(directory.value,-1);
  state_->limits=std::move(limits);state_->receipt.options=options;
  state_->receipt.format=options.format==ImportSourceFormat::Automatic?format(path.extension().string()):options.format;
  auto name=normalized(path.filename().generic_string());
  state_->receipt.root={name,read(state_->directory.value,name,state_->limits,state_->limits.max_resource_bytes)};
  state_->total=state_->receipt.root.bytes->size();
}
ImportSourceCapture::~ImportSourceCapture()=default;
const ImportSourceReceipt &ImportSourceCapture::source() const {return state_->receipt;}
std::shared_ptr<const std::vector<uint8_t>> ImportSourceCapture::resource(std::string_view path) {
  auto &s=*state_;stop(s.limits);
  if(s.finished)throw std::logic_error("Source capture is already complete");
  auto name=normalized(path);
  if(name==s.receipt.root.name)return s.receipt.root.bytes;
  if(auto i=s.names.find(name);i!=s.names.end())return s.receipt.resources[i->second].bytes;
  if(!s.limits.external_resources)throw std::runtime_error("External source resources are disabled");
  if(s.receipt.resources.size()>=511)throw std::length_error("Source resource count exceeds 512 files");
  auto bytes=read(s.directory.value,name,s.limits,s.limits.max_resource_bytes-s.total);
  // Commit the new file only after both potentially allocating containers are
  // prepared; an allocation fault cannot leave a stale resource name/index.
  auto resources=s.receipt.resources;auto names=s.names;
  names.emplace(name,resources.size());resources.push_back({name,bytes});
  s.receipt.resources=std::move(resources);s.names=std::move(names);s.total+=bytes->size();
  return bytes;
}
CapturedImportSource ImportSourceCapture::finish(std::shared_ptr<const std::vector<uint8_t>> bytes,std::string adapter,std::vector<std::string> diagnostics) {
  auto &s=*state_;stop(s.limits);
  if(s.finished || !bytes || bytes->size()<28 || bytes->size()>s.limits.max_file_bytes ||
      word(*bytes,0)!=0x46546c67 || word(*bytes,4)!=2 || word(*bytes,8)!=bytes->size() || word(*bytes,16)!=0x4e4f534a)
    throw std::length_error("Invalid canonical import adapter payload");
  auto receipt=std::make_shared<ImportSourceReceipt>(s.receipt);receipt->adapter=std::move(adapter);receipt->validate(s.limits);
  s.finished=true;return {std::move(receipt),std::move(bytes),std::move(diagnostics)};
}
CapturedImportSource capture_gltf_source(const std::filesystem::path &path,ImportOptions options,ImportLimits limits) {
  options.validate();
  if(options.units!=ImportUnits::FromFile || options.axes!=ImportAxes::FromFile ||
      options.normals!=ImportNormals::Preserve || !options.hierarchy || !options.instances ||
      !options.materials || !options.skins || !options.morphs)
    throw std::runtime_error("This glTF adapter currently requires Preserve choices; explicit conversion options remain required");
  ImportSourceCapture capture(path,options,limits);
  const auto &source=capture.source();
  if(source.format!=ImportSourceFormat::Glb && source.format!=ImportSourceFormat::Gltf)
    throw std::invalid_argument("Expected a glTF or GLB source");
  auto raw=source.root.bytes;std::span<const uint8_t> json_bytes=*raw,bin;
  if(source.format==ImportSourceFormat::Glb) {
    if(raw->size()<28 || word(*raw,0)!=0x46546c67 || word(*raw,4)!=2 || word(*raw,8)!=raw->size() || word(*raw,16)!=0x4e4f534a)
      throw std::invalid_argument("Invalid GLB source header");
    size_t n=word(*raw,12);if(n%4 || n>raw->size()-20)throw std::invalid_argument("Invalid GLB JSON bounds");
    json_bytes=std::span(*raw).subspan(20,n);
    for(size_t at=20+n;at<raw->size();) {
      if(raw->size()-at<8)throw std::invalid_argument("Invalid GLB chunk header");
      size_t size=word(*raw,at);auto type=word(*raw,at+4);
      if(size%4 || size>raw->size()-at-8 || type==0x4e4f534a || (type==0x004e4942&&!bin.empty()))
        throw std::invalid_argument("Invalid GLB chunk bounds");
      if(type==0x004e4942)bin=std::span(*raw).subspan(at+8,size);
      at+=8+size;
    }
  }
  if(json_bytes.size()>16*1024*1024)throw std::length_error("Source glTF JSON exceeds 16MiB");
  size_t visits=0;
  auto json=nlohmann::json::parse(json_bytes.begin(),json_bytes.end(),[&](int depth,nlohmann::json::parse_event_t,const nlohmann::json&){
    if(depth>64 || ++visits>limits.max_metadata_visits)throw std::length_error("Source glTF metadata budget exceeded");
    if((visits&4095)==0)stop(limits);
    return true;
  });
  if(!json.is_object())throw std::invalid_argument("Source glTF JSON must be an object");
  auto buffers=json.value("buffers",nlohmann::json::array());
  bool external=false;
  for(const auto &b:buffers)external|=b.contains("uri")&&!b.at("uri").get<std::string>().starts_with("data:");
  for(const auto &im:json.value("images",nlohmann::json::array()))external|=im.contains("uri")&&!im.at("uri").get<std::string>().starts_with("data:");
  if(source.format==ImportSourceFormat::Glb && !external)
    return capture.finish(raw,"glTF2 immutable GLB passthrough 1");
  // Buffers are merged only for core/known-buffer-independent extensions. An
  // unknown optional extension may contain buffer IDs; guessing would corrupt
  // its provenance even when the renderer ignores that extension.
  const std::set<std::string> safe={"KHR_materials_unlit","KHR_texture_transform","KHR_materials_emissive_strength","KHR_lights_punctual","VRM","VRMC_vrm","VRMC_materials_mtoon","VRMC_springBone","VRMC_node_constraint"};
  for(const auto &e:json.value("extensionsUsed",nlohmann::json::array()))
    if(!safe.contains(e.get<std::string>()))throw std::runtime_error("External glTF adapter cannot safely rebase buffers used by extension: "+e.get<std::string>());
  std::vector<uint8_t> merged;std::vector<size_t> offsets,lengths;
  auto append=[&](std::span<const uint8_t> bytes) {
    stop(limits);while(merged.size()%4)merged.push_back(0);
    if(bytes.size()>limits.max_resource_bytes-std::min(merged.size(),limits.max_resource_bytes))
      throw std::length_error("Canonical GLB binary byte budget exceeded");
    size_t at=merged.size();merged.insert(merged.end(),bytes.begin(),bytes.end());return at;
  };
  for(size_t i=0;i<buffers.size();++i) {
    stop(limits);const auto &b=buffers.at(i);size_t count=integer(b.at("byteLength"));
    std::vector<uint8_t> embedded;std::shared_ptr<const std::vector<uint8_t>> file;
    std::span<const uint8_t> bytes;
    if(b.contains("uri")) {
      auto uri=b.at("uri").get<std::string>();
      if(uri.starts_with("data:")){embedded=data_uri(uri,limits);bytes=embedded;}
      else {file=capture.resource(uri_path(uri));bytes=*file;}
    } else {
      if(i!=0 || source.format!=ImportSourceFormat::Glb)throw std::invalid_argument("Missing glTF buffer URI");
      bytes=bin;
    }
    if(count>bytes.size())throw std::invalid_argument("Truncated captured glTF buffer");
    offsets.push_back(append(bytes.first(count)));lengths.push_back(count);
  }
  if(json.contains("bufferViews"))for(auto &view:json.at("bufferViews")) {
    size_t id=integer(view.at("buffer")),offset=integer(view.value("byteOffset",nlohmann::json(0))),count=integer(view.at("byteLength"));
    if(id>=offsets.size() || offset>lengths[id] || count>lengths[id]-offset)
      throw std::invalid_argument("Source glTF buffer view exceeds captured buffer");
    view["buffer"]=0;view["byteOffset"]=offsets[id]+offset;
  }
  if(json.contains("images"))for(auto &image:json.at("images")) {
    if(!image.contains("uri"))continue;
    auto uri=image.at("uri").get<std::string>();if(uri.starts_with("data:"))continue;
    auto file=capture.resource(uri_path(uri));auto offset=append(*file);
    if(!json.contains("bufferViews"))json["bufferViews"]=nlohmann::json::array();
    auto id=json["bufferViews"].size();json["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",file->size()}});
    std::string mime;
    constexpr uint8_t png[]={137,80,78,71,13,10,26,10};
    if(file->size()>=8&&std::equal(std::begin(png),std::end(png),file->begin()))mime="image/png";
    else if(file->size()>=3&&(*file)[0]==255&&(*file)[1]==216&&(*file)[2]==255)mime="image/jpeg";
    else throw std::runtime_error("Captured image is neither standard PNG nor JPEG: "+uri);
    if(image.contains("mimeType") && image.at("mimeType")!=mime)throw std::invalid_argument("Captured image MIME type does not match its bytes");
    image.erase("uri");image["bufferView"]=id;image["mimeType"]=mime;
  }
  json["buffers"]=nlohmann::json::array({{{"byteLength",merged.size()}}});
  auto text=json.dump();while(text.size()%4)text+=' ';while(merged.size()%4)merged.push_back(0);
  if(text.size()>16*1024*1024 || text.size()+merged.size()+28>limits.max_file_bytes || text.size()+merged.size()+28>UINT32_MAX)
    throw std::length_error("Canonical GLB exceeds native source archive byte budget");
  auto result=std::make_shared<std::vector<uint8_t>>();result->reserve(text.size()+merged.size()+28);
  append_word(*result,0x46546c67);append_word(*result,2);append_word(*result,uint32_t(text.size()+merged.size()+28));
  append_word(*result,uint32_t(text.size()));append_word(*result,0x4e4f534a);result->insert(result->end(),text.begin(),text.end());
  append_word(*result,uint32_t(merged.size()));append_word(*result,0x004e4942);result->insert(result->end(),merged.begin(),merged.end());
  return capture.finish(result,"glTF2 captured core resource rebase 1");
}
} // namespace dome
