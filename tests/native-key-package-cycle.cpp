// SPDX-License-Identifier: GPL-3.0-or-later
// Integration of real owning keys, source paint, namespace append and file workers.
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include "dome/native-authoring.hpp"
#include "dome/native-scene-append.hpp"
#include "dome/shapekey-authoring.hpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace dome;
namespace {
size_t checks=0;
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
std::vector<uint8_t> read(const std::filesystem::path &path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot open integration artifact");return {std::istreambuf_iterator<char>(in),{}};}
void settle(Document &document){require(document.wait_asset(std::chrono::seconds(180))&&document.wait_mesh(std::chrono::seconds(180)),"Ordered document work reaches its bounded endpoint");if(!document.frame().state->error.empty())throw std::runtime_error(document.frame().state->error);}
template<class T>bool bits(const std::vector<T>&a,const std::vector<T>&b){return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);}
void same_mesh(const SculptObject &a,const SculptObject &b){require(bool(a.painted)&&bool(b.painted),"Both namespace endpoints retain writable mesh authority");const auto &x=a.painted->mesh(),&y=b.painted->mesh();require(bits(x.positions,y.positions)&&bits(x.normals,y.normals)&&bits(x.uv,y.uv)&&x.indices==y.indices,"Appending changes no current REST vertex/corner geometry bits");}
void cycle(const std::filesystem::path &key_file,const std::filesystem::path &paint_file,const std::filesystem::path &out){
 std::filesystem::create_directories(out);auto keys=Scene::load(key_file),paint=Scene::load(paint_file);const auto key_exact=keys.encode(),paint_exact=paint.encode();
 require(key_exact.at("version")==29&&keys.active().material_bindings&&keys.active().material_bindings->key_derivation&&keys.active().shape_source,"Integration input is a genuine owning Scene29 converted target with retained source");
 Scene destination;destination.active().visible=false;const auto destination_exact=destination.encode();auto first=append_native_scene(destination,keys);size_t active_index=0;for(size_t i=0;i<keys.objects().size();++i)if(keys.objects()[i].id==keys.active().id)active_index=i;const auto key_id=first.objects.at(active_index);
 require(first.objects.size()==keys.objects().size()&&key_id!=destination.active().id,"Owning key objects append into a distinct live object namespace");
 const auto &key=first.scene.object(key_id);require(key.appearance==keys.active().appearance&&key.material_bindings==keys.active().material_bindings&&key.deformation==keys.active().deformation,"Namespace append preserves exact immutable appearance, key-frame and deformation owners");same_mesh(key,keys.active());
 auto second=append_native_scene(first.scene,paint);auto combined=std::move(second.scene);const auto exact=combined.encode();require(exact.at("version")==29&&combined.objects().size()==destination.objects().size()+keys.objects().size()+paint.objects().size(),"Scene29 owning keys and genuine source-aware paint coexist after two native appends");
 require(destination.encode()==destination_exact&&keys.encode()==key_exact&&paint.encode()==paint_exact,"Both detached appends preserve every original input Scene byte");
 const auto native=out/"combined.dsculpt";combined.save(native);require(Scene::load(native).encode()==exact,"Combined native archive restarts with exact key, source-paint and hidden-object state");
 Document document;document.replace(combined);settle(document);const auto package=out/"editable.glb";document.export_asset(package,AssetExportFormat::EditableGlb256).get();const auto emitted=read(package);auto recovered=extract_native_authoring(emitted);require(recovered&&recovered->encode()==exact,"Actual native export worker embeds the exact combined Scene29 project");require(document.copy_scene().encode()==exact,"Portable compilation leaves all current/retained key and paint authority unchanged");
 Document opened;opened.open_async(package).get();settle(opened);require(opened.frame().state->project_path.empty()&&opened.frame().state->dirty&&opened.copy_scene().encode()==exact,"Actual Open restores combined native authoring as unsaved without adopting GLB path");const auto saved=out/"recovered.dsculpt";opened.save_async(saved).get();require(Scene::load(saved).encode()==exact&&read(package)==emitted,"Subsequent native Save preserves every editable source and the original GLB bytes");
 Document importer;importer.replace(destination);settle(importer);ImportSettings options;options.paint_resolution=64;importer.import_asset(package,options,importer.begin_import());settle(importer);const auto imported=importer.copy_scene().encode();require(importer.copy_scene().objects().size()==destination.objects().size()+combined.objects().size(),"Actual Import recovers every hidden and visible packaged object before namespace append");require(importer.undo()&&importer.copy_scene().encode()==destination_exact,"Import of owning keys and source paint remains one exact Undo");require(importer.redo()&&importer.copy_scene().encode()==imported,"Redo restores the complete imported owning-key/source-paint project");
 auto before_restore=combined.snapshot();ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;auto original=keys.snapshot();author_shapekey(original,original.active().id,restore);author_shapekey(before_restore,key_id,restore);same_mesh(before_restore.object(key_id),original.active());require(before_restore.object(key_id).material_bindings==original.active().material_bindings&&before_restore.object(key_id).deformation==original.active().deformation,"Restore after append follows genuine pre-key material and deformation authority");before_restore.validate();const auto restored=out/"restored.dsculpt";before_restore.save(restored);require(Scene::load(restored).encode()==before_restore.encode(),"Restored pre-key source remains saveable and restartable in the rebased namespace");
 require(keys.encode()==key_exact&&paint.encode()==paint_exact&&document.copy_scene().encode()==exact,"Complete export/Open/Import/Restore cycle never edits retained input owners");
}
}
int main(int argc,char **argv)try{if(argc<4)return 2;std::filesystem::path out=argv[1];for(int i=3;i<argc;++i)cycle(argv[i],argv[2],out/("case"+std::to_string(i-3)));std::cout<<"PASS "<<checks<<" combined key/native-append/file-cycle checks\n";return 0;}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<std::endl;return 1;}
