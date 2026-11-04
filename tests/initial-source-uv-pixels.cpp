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
 if(argc!=2)return 2;auto out=std::filesystem::path(argv[1]);std::filesystem::create_directories(out);size_t checked=0,sensitive=0;Scene before;auto original_scene=before.encode();
 for(int mode:{10497,33648,33071}){auto fixture=source(mode);auto path=out/(std::to_string(mode)+"-raw.glb");fixture.save(path);ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;auto prepared=prepare_authoring_import(before,path,options);
  for(auto id:prepared.objects){const auto &object=prepared.scene.object(id);const auto &mesh=object.painted->mesh();const auto &raw=object.appearance->asset->primitives[0].mesh;const auto &paint=*object.painted;const auto &map=*paint.source_maps()->color.image;const auto original=paint.source_sampling_original();const auto vertices=paint.source_sampling_vertices();const uint32_t grid=paint.resolution();
   check(grid==64&&paint.source_sampling_prepared()&&original&&vertices.size()==mesh.positions.size(),"Initial writable atlas has its declared resolution and genuine original-coordinate owner");
   check(map.width==2&&map.height==2&&map.rgba==std::vector<uint8_t>({255,0,0,32,0,255,0,96,0,0,255,160,255,255,255,255})&&paint.source_maps()->color.wrap_s==mode&&paint.source_maps()->color.wrap_t==mode,"Original image bytes and actual repeat, mirror or clamp sampler are retained without resampling");
   std::vector<uint8_t> visited(size_t(grid)*grid);
   for(size_t face=0;face<mesh.indices.size();face+=3){const size_t face_before=checked;const auto a=mesh.uv[mesh.indices[face]],b=mesh.uv[mesh.indices[face+1]],c=mesh.uv[mesh.indices[face+2]];const double den=(double(b[1])-c[1])*(double(a[0])-c[0])+(double(c[0])-b[0])*(double(a[1])-c[1]);check(den!=0,"Independent atlas oracle sees genuine nondegenerate destination triangles");
    for(uint32_t y=0;y<grid;++y)for(uint32_t x=0;x<grid;++x){const double u=(x+.5)/grid,v=(y+.5)/grid;const double w0=((double(b[1])-c[1])*(u-c[0])+(double(c[0])-b[0])*(v-c[1]))/den,w1=((double(c[1])-a[1])*(u-c[0])+(double(a[0])-c[0])*(v-c[1]))/den,w2=1-w0-w1;if(std::min({w0,w1,w2})<1e-6)continue;size_t at=size_t(y)*grid+x;if(visited[at])continue;visited[at]=1;const std::array<double,3> weights={w0,w1,w2};double su=0,sv=0,retained_u=0,retained_v=0;for(int k=0;k<3;++k){auto source_vertex=object.appearance->source_vertices.at(mesh.indices[face+k]);su+=raw.uv.at(source_vertex)[0]*weights[k];sv+=raw.uv.at(source_vertex)[1]*weights[k];const auto retained=original->mesh().uv.at(vertices[mesh.indices[face+k]]);retained_u+=retained[0]*weights[k];retained_v+=retained[1]*weights[k];}
     check(std::abs(su-retained_u)<=1e-12&&std::abs(sv-retained_v)<=1e-12,"Independent accessor-to-atlas oracle retains exact original coordinates at each interior sample");
     auto color=expected(su,sv,mode),transported=expected(retained_u,retained_v,mode);
     check(color==transported,"Every transported coordinate samples the original nearest image without an intermediate quantized atlas");
     sensitive+=color!=expected(u,v,mode);
     ++checked;
    }
    check(checked-face_before>=.75*std::abs(den)*grid*grid*.5,"Every destination triangle contributes its expected raster interior to the independent oracle");
   }
  }
 }
 check(checked>15000&&sensitive>1000,"Independent source-coordinate oracle covers all three wrap modes and rejects substituting writable atlas coordinates");check(before.encode()==original_scene,"Independent sampling validation never mutates its caller");std::cout<<"Independent initial atlas coordinate samples "<<checked<<" wrong-domain differences "<<sensitive<<std::endl;return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
