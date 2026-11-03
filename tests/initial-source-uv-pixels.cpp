// SPDX-License-Identifier: GPL-3.0-or-later
#define main initial_source_uv_workflow
#include "initial-source-uv.cpp"
#undef main
namespace {
double wrapped(double value,int mode){if(mode==33071)return std::clamp(value,0.,1.);if(mode==33648){double phase=std::fmod(value,2.);if(phase<0)phase+=2.;return phase<1?phase:2-phase;}return value-std::floor(value);}
std::array<uint8_t,4> expected(double u,double v,int mode){
 constexpr std::array<std::array<uint8_t,4>,4> original={{{255,0,0,32},{0,255,0,96},{0,0,255,160},{255,255,255,255}}};
 const int x=std::clamp(int(std::floor(wrapped(u,mode)*2)),0,1),y=std::clamp(int(std::floor(wrapped(v,mode)*2)),0,1);return original[y*2+x];
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;auto out=std::filesystem::path(argv[1]);std::filesystem::create_directories(out);size_t checked=0,ties=0;Scene before;auto original_scene=before.encode();
 for(int mode:{10497,33648,33071}){auto fixture=source(mode);auto path=out/(std::to_string(mode)+"-raw.glb");fixture.save(path);ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;auto prepared=prepare_authoring_import(before,path,options);
  for(auto id:prepared.objects){const auto &object=prepared.scene.object(id);const auto &mesh=object.painted->mesh();const auto &raw=object.appearance->asset->primitives[0].mesh;const auto &map=*object.painted->source_maps()->color.image;check(map.width==64&&map.height==64,"Initial editable atlas uses its declared output grid");std::vector<uint8_t> visited(size_t(map.width)*map.height);
   for(size_t face=0;face<mesh.indices.size();face+=3){const size_t face_before=checked;const auto a=mesh.uv[mesh.indices[face]],b=mesh.uv[mesh.indices[face+1]],c=mesh.uv[mesh.indices[face+2]];const double den=(double(b[1])-c[1])*(double(a[0])-c[0])+(double(c[0])-b[0])*(double(a[1])-c[1]);check(den!=0,"Independent atlas oracle sees genuine nondegenerate destination triangles");
    for(uint32_t y=0;y<map.height;++y)for(uint32_t x=0;x<map.width;++x){const double u=(x+.5)/map.width,v=(y+.5)/map.height;const double w0=((double(b[1])-c[1])*(u-c[0])+(double(c[0])-b[0])*(v-c[1]))/den,w1=((double(c[1])-a[1])*(u-c[0])+(double(a[0])-c[0])*(v-c[1]))/den,w2=1-w0-w1;if(std::min({w0,w1,w2})<1e-6)continue;size_t at=size_t(y)*map.width+x;if(visited[at])continue;visited[at]=1;const std::array<double,3> weights={w0,w1,w2};double su=0,sv=0;for(int k=0;k<3;++k){auto source_vertex=object.appearance->source_vertices.at(mesh.indices[face+k]);su+=raw.uv.at(source_vertex)[0]*weights[k];sv+=raw.uv.at(source_vertex)[1]*weights[k];}
     auto color=expected(su,sv,mode);bool matches=true;for(int channel=0;channel<4;++channel)matches&=std::abs(int(map.rgba[at*4+channel])-int(color[channel]))<=1;
     // A geometric texture boundary can occupy an entire row; its frequency
     // is not a colour error. Every tie still obeys the fixed coordinate and
     // per-channel bounds below, and all non-edge samples must match directly.
     if(!matches){bool tie=false;for(double du:{-2e-5,0.,2e-5})for(double dv:{-2e-5,0.,2e-5}){auto alternate=expected(su+du,sv+dv,mode);bool equal=true;for(int channel=0;channel<4;++channel)equal&=std::abs(int(map.rgba[at*4+channel])-int(alternate[channel]))<=1;tie|=equal;}check(tie,"Every atlas texel matches the independent original sampler, allowing only declared nearest-edge ties");++ties;}
     ++checked;
    }
    check(checked-face_before>=.75*std::abs(den)*map.width*map.height*.5,"Every destination triangle contributes its expected raster interior to the independent oracle");
   }
  }
 }
 check(checked>15000,"Independent sampler covers all three wrap modes and their genuine nearest-edge cases");check(before.encode()==original_scene,"Independent sampling validation never mutates its caller");std::cout<<"Independent initial atlas texels "<<checked<<" nearest edge ties "<<ties<<std::endl;return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
