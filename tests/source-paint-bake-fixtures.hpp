// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
// Include the shared native graphics fixture first; its Pixel/Fbo/check types
// are the independent render oracle used by both portable export consumers.
#include "source-paint-fixtures.hpp"
namespace {
struct Fidelity {double mean=0;size_t visible=0,interior=0;int maximum=0,interior_maximum=0;};
Fidelity fidelity(const Pixels &reference,const Pixels &actual){
 constexpr int side=Fbo::size;std::vector<bool> edge(side*side),footprint(side*side);
 for(int y=0;y<side;++y)for(int x=0;x<side;++x){auto at=y*side+x;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){auto nx=x+dx,ny=y+dy;if(nx<0||nx>=side||ny<0||ny>=side)continue;for(int c=0;c<4;++c)if(std::abs(int(reference[at*4+c])-int(reference[(ny*side+nx)*4+c]))>16)edge[at]=true;}}
 for(int y=0;y<side;++y)for(int x=0;x<side;++x)if(edge[y*side+x])for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){auto nx=x+dx,ny=y+dy;if(nx>=0&&nx<side&&ny>=0&&ny<side)footprint[ny*side+nx]=true;}
 Fidelity out;double total=0;for(size_t at=0;at<edge.size();++at){if(!reference[at*4+3]&&!actual[at*4+3])continue;++out.visible;if(!footprint[at])++out.interior;for(int c=0;c<4;++c){auto error=std::abs(int(reference[at*4+c])-int(actual[at*4+c]));total+=error;out.maximum=std::max(out.maximum,error);if(!footprint[at])out.interior_maximum=std::max(out.interior_maximum,error);}}
 if(out.visible)out.mean=total/(out.visible*4);return out;
}
void settle(Document &d){check(d.wait_mesh()&&d.frame().state->error.empty(),"Actual source-aware bake project reaches native rendering");}
void ink(Scene &scene){auto &o=scene.active();auto p=o.painted->snapshot();
 for(auto channel:{PaintChannel::Color,PaintChannel::Metallic,PaintChannel::Roughness,PaintChannel::Emission}){
  p->add_layer("Independent field");PaintSettings ink;ink.front_only=false;ink.channel=channel;ink.color={.1f,.7f,.3f,.7f};ink.metallic=.8f;ink.roughness=.3f;ink.emission={.4f,.2f,.1f};
  Brush b;b.from=b.to={.02f,.01f,0};b.normal={0,0,1};b.radius=.13f;b.strength=.45f;p->begin_stroke();p->stamp(b,ink);check(p->end_stroke(),"Real editable source paint contributes its independently covered channel");p->layer_properties("Independent field",true,.65f);
 }
 p->add_layer("Hidden field");p->layer_properties("Hidden field",false,.3f);o.painted=std::move(p);
}
inline import_fixture::Fixture portable_material_fixture(int kind) {
 auto fixture=source_paint_fixture::input(kind==6?0:kind==7?2:kind);auto &material=fixture.j["materials"][0];material["extensions"].erase("KHR_materials_unlit");
  if(kind==6){
   Image normal{2,2,{255,128,255,255,128,255,255,255,32,128,255,255,128,32,255,255}};auto png=normal.png();while(fixture.bin.size()%4)fixture.bin.push_back(0);const auto view=fixture.j["bufferViews"].size();fixture.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",fixture.bin.size()},{"byteLength",png.size()}});fixture.bin.insert(fixture.bin.end(),png.begin(),png.end());const auto image=fixture.j["images"].size();fixture.j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});const auto texture=fixture.j["textures"].size();fixture.j["textures"].push_back({{"source",image},{"sampler",0}});
   material["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"]=1;material["pbrMetallicRoughness"]["metallicRoughnessTexture"]={{"index",0},{"texCoord",0}};material["normalTexture"]={{"index",texture},{"texCoord",0},{"scale",.6f}};material["occlusionTexture"]={{"index",0},{"texCoord",1},{"strength",.4f}};
  }
  if(kind==7){auto &shade=material["extensions"]["VRMC_materials_mtoon"]["shadeMultiplyTexture"];shade["texCoord"]=0;shade["extensions"]["KHR_texture_transform"]["texCoord"]=0;}
 return fixture;
}

}
