// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/color-preparation.hpp"
#include <iostream>
#include <chrono>
#include <thread>
// Read real bookkeeping used by the production128MiB cap, with no product test
// hook/API/layout change. Explicit-instantiation access is confined to this TU.
namespace inspect {
struct Count {using type=size_t dome::PaintedMesh::*;friend type field(Count);};
struct BeforeCount {using type=size_t dome::PaintedMesh::*;friend type field(BeforeCount);};
struct CommonCount {using type=size_t dome::PaintedMesh::*;friend type field(CommonCount);};
struct Scratch {using type=std::vector<std::shared_ptr<const dome::PaintColorTile>> dome::PaintedMesh::*;friend type field(Scratch);};
struct Swap {using type=void(dome::PaintedMesh::*)(dome::PaintedMesh&)noexcept;friend type field(Swap);};
template<class Tag,typename Tag::type pointer>struct Access {friend typename Tag::type field(Tag){return pointer;}};
template struct Access<Count,&dome::PaintedMesh::allocated_color_tiles_>;
template struct Access<BeforeCount,&dome::PaintedMesh::before_allocated_color_tiles_>;
template struct Access<CommonCount,&dome::PaintedMesh::allocated_tiles_>;
template struct Access<Scratch,&dome::PaintedMesh::before_colors_>;
template struct Access<Swap,&dome::PaintedMesh::swap_state>;
size_t count(const dome::PaintedMesh&p){return p.*field(Count{});}size_t before(const dome::PaintedMesh&p){return p.*field(BeforeCount{});}size_t common(const dome::PaintedMesh&p){return p.*field(CommonCount{});}
const auto &scratch(const dome::PaintedMesh&p){return p.*field(Scratch{});}
void swap(dome::PaintedMesh&a,dome::PaintedMesh&b){(a.*field(Swap{}))(b);}
}
using namespace dome;
namespace {
unsigned checks=0;void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Scene source(){Scene scene;auto &o=scene.active();o.painted=std::make_shared<PaintedMesh>(quad(),64);o.mesh_active=true;ColorPreparationOptions options;options.resolve_identity=true;auto prepared=prepare_source_colors({o.painted,{},{},{},o.material},options);o.painted=prepared.paint;o.material_bindings=prepared.material_bindings;scene.validate();return scene;}
void ready(Document &d){if(!d.wait_asset()||!d.wait_native_stroke()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("History fixture: "+d.frame().state->error);}
PaintSettings settings(){PaintSettings s;s.channel=PaintChannel::Color;s.color={.9f,.2f,.5f,.7f};s.flow=1;return s;}
NativeStrokeSample sample(){NativeStrokeSample s;s.origin={0,0,1};s.direction={0,0,-1};s.brush.radius=.2f;s.brush.pressure=s.brush.strength=1;return s;}
void paint(Document &d,bool accept=true){d.paint_settings(settings());check(d.begin_desktop(),"Actual shared Document/XR stroke-history gesture starts");Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=.2f;d.stamp_desktop(brush);check(inspect::count(*d.copy_scene().active().painted)>0,"Actual synchronous pressure mutation allocates PBR owners before acceptance/cancel");if(accept)d.end_desktop();else d.cancel_desktop();ready(d);}

void accounting(const PaintedMesh &p,const char *why){size_t colors=0,common=0;for(const auto &l:p.layers()){for(const auto &t:l.pbr_tiles)colors+=bool(t);for(const auto &t:l.tiles)common+=bool(t);}std::cout<<"ACCOUNTING public="<<colors<<" actual="<<inspect::count(p)<<" common="<<common<<'/'<<inspect::common(p)<<std::endl;check(inspect::count(p)==colors&&inspect::common(p)==common,why);const size_t planes=(p.color_projection()?p.color_projection()->vertex_rgba->rgba.size():0)+(p.baked()?p.baked()->storage_bytes():0);check(p.storage_bytes()==planes+inspect::common(p)*sizeof(PaintTile)+inspect::count(p)*sizeof(PaintColorTile),"Real cap bookkeeping equals independently enumerated persistent storage");}
}
int main(int argc,char **argv)try{
 const auto directory=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"pbr-history-accounting";std::filesystem::create_directories(directory);const auto original=source();const auto encoded=original.encode();Document d;d.replace(original);ready(d);accounting(*d.copy_scene().active().painted,"Initial prepared PBR budget is exact");
 paint(d);const auto accepted=d.copy_scene().encode();auto painted=d.copy_scene().active().painted;check(inspect::count(*painted)>0,"Genuine prepared painting allocates independent sparse PBR tiles");accounting(*painted,"Accepted paint budget matches actual retained tiles");
 check(d.undo(),"Actual Document Undo accepts the native PBR history entry");ready(d);check(d.copy_scene().encode()==encoded,"Undo preserves exact original paint/material/Scene archive");accounting(*d.copy_scene().active().painted,"Document Undo restores PBR budget counters with the actual sparse owners");
 for(unsigned cycle=0;cycle<4;++cycle){check(d.redo(),"Actual Document Redo restores accepted PBR content");ready(d);check(d.copy_scene().encode()==accepted,"Redo retains exact accepted source and paint bytes");accounting(*d.copy_scene().active().painted,"Redo PBR budget remains exact across repeated cycles");check(d.undo(),"Undo returns to the same original sparse budget");ready(d);accounting(*d.copy_scene().active().painted,"Repeated Undo does not leak phantom PBR allocation");paint(d,false);check(d.copy_scene().encode()==encoded,"Cancelled new native paint preserves original content and prior Redo");accounting(*d.copy_scene().active().painted,"Cancelled gesture preserves exact persistent budget");}
 check(d.redo(),"Cancelled work preserves successful earlier Redo");ready(d);auto path=directory/"accepted.dsculpt";d.save(path);auto loaded=Scene::load(path);check(loaded.encode()==d.copy_scene().encode(),"Actual Save/reopen retains exact accepted PBR state");accounting(*loaded.active().painted,"Decoded budget equals live Document accounting");check(loaded.active().painted->storage_bytes()==d.copy_scene().active().painted->storage_bytes(),"Restart cannot silently repair a different live budget");
 // The real asynchronous desktop path uses whole-Scene history. Keep it
 // covered too; it is not the source of the inherited in-place swap defect.
 auto before_native=d.copy_scene().encode();d.paint_settings(settings());check(d.begin_native_stroke()&&d.sample_native_stroke(sample()),"Actual queued native PBR gesture is admitted");d.end_native_stroke();ready(d);accounting(*d.copy_scene().active().painted,"Queued native acceptance preserves exact PBR budget");check(d.undo(),"Queued native history Undo succeeds");ready(d);check(d.copy_scene().encode()==before_native,"Queued native Undo restores exact accepted pre-gesture data");accounting(*d.copy_scene().active().painted,"Queued native Undo remains exact alongside legacy target-identity history");
 // Exercise the exact noexcept primitive while both objects own real active
 // edits, so scratch colour buffers and their before counts differ.
 auto a=source().active().painted->snapshot(),b=source().active().painted->snapshot();Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=.2f;auto s=settings();a->begin_stroke();a->stamp(brush,s);b->begin_stroke();b->stamp(brush,s);b->end_stroke();b->begin_stroke();
 auto scratch_a=inspect::scratch(*a),scratch_b=inspect::scratch(*b);auto count_a=inspect::before(*a),count_b=inspect::before(*b);check(scratch_a!=scratch_b&&count_a!=count_b,"Direct swap fixture owns different genuine scratch/before-budget states");
 inspect::swap(*a,*b);check(inspect::scratch(*a)==scratch_b&&inspect::scratch(*b)==scratch_a&&inspect::before(*a)==count_b&&inspect::before(*b)==count_a,"Noexcept history swap exchanges active scratch colour owners and before-budget counters together");a->cancel_stroke();b->cancel_stroke();accounting(*a,"First active edit cancels to its swapped genuine before-state");accounting(*b,"Second active edit cancels to its swapped genuine before-state");check(inspect::count(*a)>0&&inspect::count(*b)==0,"Cancel restores the two distinct authored PBR histories without stale buffers");
 std::cout<<"PASS "<<checks<<" native PBR history/accounting checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
