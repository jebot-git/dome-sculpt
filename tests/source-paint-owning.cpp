// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-paint.hpp"
#include "dome/scene.hpp"
#include "dome/source-maps.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception &e){std::cout<<"EXPECTED "<<e.what()<<'\n';return true;}}
std::shared_ptr<PaintedMesh> read(const nlohmann::json &j){return PaintedMesh::decode(j,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true});}
}
int main()try {
  Mesh raw;raw.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};raw.normals.assign(4,{0,0,1});
  raw.uv={{-1.25f,-.5f},{1.75f,-.5f},{1.75f,2.5f},{-1.25f,2.5f}};raw.indices={0,1,2,0,2,3};
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,2,
    {255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255}});
  maps->normal.image=std::make_shared<const Image>(Image{1,1,{150,170,240,255}});maps->normal.texcoord=1;
  maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(raw.uv);
  maps->color.min_filter=9986;maps->color.mag_filter=9728;maps->color.wrap_s=33648;maps->color.wrap_t=10497;
  PaintSurfaceOptions original;original.policy=PaintUvPolicy::SourceAuthored;
  auto before=PaintedMesh::from_maps(raw,maps,64,original);before->add_layer("Actual initial layer metadata");
  const auto baseline=before->encode();const std::vector<uint32_t> vertices{0,1,2,0,2,3};Mesh target;
  for(auto v:vertices){target.positions.push_back(raw.positions[v]);target.normals.push_back(raw.normals[v]);}
  target.indices={0,1,2,3,4,5};target.uv={{.05f,.05f},{.45f,.05f},{.45f,.45f},{.55f,.55f},{.95f,.95f},{.55f,.95f}};
  auto paint=prepare_source_paint(before,target,vertices);Material material;
  check(before->encode()==baseline,"Preparation leaves its complete original input exact");
  check(paint->source_sampling_prepared()&&paint->source_sampling_original()->encode()==baseline&&
        std::equal(paint->source_sampling_vertices().begin(),paint->source_sampling_vertices().end(),vertices.begin()),
        "Current strict grid owns its real pristine original and exact remap");
  check(paint->mesh().uv==target.uv&&paint->source_maps()->color.image==maps->color.image&&
        paint->source_maps()->color.texcoord==2&&paint->source_maps()->normal.texcoord==1,
        "Authored UV0 and each original role remain independent without image copies");
  check(paint->layers().size()==before->layers().size()&&paint->active_layer()==before->active_layer()&&
        paint->layers()[1].name==before->layers()[1].name,"Preparation preserves actual layer IDs, names and active selection");
  check(paint->preview(material)->source_only,"Unpainted prepared preview still uses exact original GPU sampling");
  const auto empty=paint->encode();check(empty.at("version")==4,"Actual empty prepared owner emits Paint4");
  check(refuses([&]{PaintedMesh::decode(empty);})&&
        refuses([&]{PaintedMesh::decode(empty,nullptr,false,nullptr,nullptr);}),
        "Legacy decode capabilities reject Paint4 even with no allocated paint");
  auto reopened=read(empty);check(reopened->encode()==empty&&reopened->source_sampling_prepared(),
        "Owning standalone decode replays the original sampling and round-trips exactly");
  auto bare=PaintedMesh::from_maps(target,paint->source_maps(),64);
  check(refuses([&]{bare->begin_stroke();})&&!bare->in_stroke(),
        "A bare alternate-UV image map cannot fabricate writable authority");
  reopened->begin_stroke();reopened->cancel_stroke();check(reopened->encode()==empty,
        "A cancelled actual paint gesture preserves complete owner/source state");
  Brush brush;brush.from=brush.to={.5f,.5f,0};brush.normal={0,0,1};brush.radius=.35f;
  PaintSettings ink;ink.color={.1f,.8f,.3f,.6f};ink.front_only=false;
  reopened->begin_stroke();reopened->stamp(brush,ink);check(reopened->end_stroke(),"Real atlas stroke allocates actual current-grid paint");
  const auto painted=reopened->encode();check(painted!=empty&&painted.at("before")==empty.at("before"),
        "Paint changes its current field while retaining exact original sampling bytes");
  check(reopened->undo()&&reopened->encode()==empty&&reopened->redo()&&reopened->encode()==painted,
        "Internal paint history retains both complete owning states");
  auto restart=read(painted);check(restart->encode()==painted&&restart->source_sampling_prepared(),
        "Actual painted source ownership survives restart");
  before->add_layer("Later caller-only metadata");check(paint->source_sampling_original()->encode()==baseline,
        "Retained original is a detached COW snapshot even if its caller later edits");
  auto forged=painted;forged["payload"]["source_maps"]["payload"]["color"]["wrap_s"]=33071;
  check(refuses([&]{read(forged);}),"A changed current sampler cannot borrow a real original proof");
  forged=painted;forged["vertices"].get_binary()[0]=1;
  check(refuses([&]{read(forged);}),"A mismatched original corner map cannot borrow authority");
  forged=painted;forged["payload"]["uv_policy"]="source-authored";
  check(refuses([&]{read(forged);}),"Paint4 cannot substitute a linked source raster for its strict independent grid");
  forged=painted;forged["before"]["version"]=4;
  check(refuses([&]{read(forged);}),"Nested source envelopes refuse before recursive construction");
  forged=painted;forged["payload"]["layers"][0]["physical_emission_tiles"]=nlohmann::json::array();
  check(refuses([&]{read(forged);}),"Source capability cannot silently admit unknown physical-emission payloads");
  forged=painted;forged["before"]["layers"][0]["tiles"].push_back({{"index",0},
    {"data",nlohmann::json::binary(std::vector<uint8_t>(32*32*14))}});
  check(refuses([&]{read(forged);}),"Even hidden or empty-looking original paint data invalidates pristine source authority");
  auto payload=painted.at("payload");payload["source_images"]=painted.at("source_images");
  check(refuses([&]{read(payload);}),"Removing the actual source owner cannot retain independent editable paint");
  SourcePaintOptions limits;limits.max_working_bytes=1;
  check(refuses([&]{prepare_source_paint(paint->source_sampling_original(),target,vertices,limits);}),
        "Cumulative preparation memory refuses before dense raster construction");
  limits={};limits.cancelled=[]{return true;};
  check(refuses([&]{prepare_source_paint(paint->source_sampling_original(),target,vertices,limits);}),
        "Cancelled preparation never publishes a new owner");
  check(refuses([&]{prepare_source_paint(restart,target,vertices);}),
        "Initial preparation never pretends to transfer existing paint");
  PaintImageArchive images;auto shared=restart->encode(&images);
  check(!shared.contains("source_images")&&PaintedMesh::decode(shared,&images,false,nullptr,nullptr,SourcePaintDecodeContext{true})->encode()==painted,
        "Shared image archive preserves actual original image identity and standalone output");
  std::cout<<"PASS "<<checks<<" owning source-paint data checks; no native renderer/Scene admission claim\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
