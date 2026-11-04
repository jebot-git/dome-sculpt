// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
// Test-only independent dense reference, derived from persisted sparse tile
// bytes and geometric UV coverage. No production overlay compiler, padding,
// point sampler, compositor, renderer material helper or readback is used.
struct BackgroundPaintOracle {
 uint32_t size=0;std::array<std::vector<float>,4> planes;
 explicit BackgroundPaintOracle(const dome::PaintedMesh &paint):size(paint.resolution()){
  if(paint.baked()||paint.color_projection()||paint.normal_projection())throw std::runtime_error("Expression paint oracle needs its actual sparse Source4 domain");
  const size_t count=size_t(size)*size;for(auto &plane:planes)plane.resize(count*4);
  const auto &mesh=paint.mesh();std::vector<size_t> valid;std::vector<bool> inside(count);
  for(size_t y=0;y<size;++y)for(size_t x=0;x<size;++x){const double px=(x+.5)/size,py=(y+.5)/size;
   for(size_t c=0;c<mesh.indices.size();c+=3){const auto a=mesh.uv[mesh.indices[c]],b=mesh.uv[mesh.indices[c+1]],d=mesh.uv[mesh.indices[c+2]];
    const double ax=b[0]-a[0],ay=b[1]-a[1],bx=d[0]-a[0],by=d[1]-a[1],area=ax*by-ay*bx;if(std::abs(area)<1e-20)continue;
    const double qx=px-a[0],qy=py-a[1],u=(qx*by-qy*bx)/area,v=(ax*qy-ay*qx)/area;
    if(u>=-1e-7&&v>=-1e-7&&u+v<=1+1e-7){inside[y*size+x]=true;valid.push_back(y*size+x);break;}
   }
  }
  if(valid.empty())throw std::runtime_error("Independent writable paint raster is empty");
  auto decode=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
  for(const auto pixel:valid){const size_t x=pixel%size,y=pixel/size,tile=(y/32)*(size/32)+x/32,within=(y%32)*32+x%32;std::array<double,4> color{},emission{},coverage{},physical{};
   for(const auto &layer:paint.layers())if(layer.visible&&layer.opacity>0&&layer.tiles.at(tile)){
    const auto &data=layer.tiles.at(tile)->pixels[within].data;const double mask=data[13]/255.*layer.opacity,ca=data[9]/255.*mask;double ea=data[12]/255.*mask;const dome::PaintEmissionValue *linear=(!layer.emission_tiles.empty()&&layer.emission_tiles.at(tile))?&layer.emission_tiles.at(tile)->pixels[within]:nullptr;if(linear)ea=linear->coverage*mask;
    if(!layer.pbr_tiles.empty()&&layer.pbr_tiles.at(tile))throw std::runtime_error("Oracle unexpectedly received a prepared PBR color plane");
    for(size_t k=0;k<4;++k)color[k]=color[k]*(1-ca)+(k==3?data[k]/255.:decode(data[k]/255.))*ca;
    for(size_t k=0;k<3;++k){emission[k]=emission[k]*(1-ea)+(linear?linear->legacy[k]:decode(data[k+6]/255.))*ea;physical[k]=physical[k]*(1-ea)+(linear?linear->physical[k]:0)*ea;}
    coverage[0]=coverage[0]*(1-ca)+ca;coverage[3]=coverage[3]*(1-ea)+ea;
   }
   for(size_t k=0;k<4;++k){planes[0][pixel*4+k]=float(color[k]);planes[1][pixel*4+k]=float(emission[k]);planes[2][pixel*4+k]=float(coverage[k]);planes[3][pixel*4+k]=float(physical[k]);}
  }
  for(size_t pixel=0;pixel<count;++pixel)if(!inside[pixel]){size_t nearest=valid.front();double distance=1e30;
   for(auto candidate:valid){const double dx=double(pixel%size)-double(candidate%size),dy=double(pixel/size)-double(candidate/size),d=dx*dx+dy*dy;if(d<distance){distance=d;nearest=candidate;}}
   for(auto &plane:planes)for(size_t k=0;k<4;++k)plane[pixel*4+k]=plane[nearest*4+k];
  }
 }
 std::array<GLuint,4> upload(GLuint program)const{
  std::array<GLuint,4> images;glGenTextures(4,images.data());for(size_t i=0;i<4;++i){glActiveTexture(GL_TEXTURE3+i);glBindSampler(3+i,0);glBindTexture(GL_TEXTURE_2D,images[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,size,size,0,GL_RGBA,GL_FLOAT,planes[i].data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glUniform1i(glGetUniformLocation(program,std::array<const char*,4>{"paintColor","paintEmission","paintCoverage","paintPhysical"}[i]),3+i);}return images;
 }
};
