// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace dome;
using Rgb=std::array<double,3>;
Rgb physical(const PaintedMesh &paint,double u,double v){
 const int r=int(paint.resolution());const double x=u*r-.5,y=v*r-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));Rgb result{};
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto px=std::clamp(ix+dx,0,r-1),py=std::clamp(iy+dy,0,r-1);Rgb texel{};
  for(const auto &layer:paint.layers())if(layer.visible&&layer.opacity){const auto e=paint.emission_pixel(layer.id,px,py);const double coverage=double(e.coverage)*paint.pixel(layer.id,px,py).data[13]/255.*layer.opacity;
   for(size_t c=0;c<3;++c)texel[c]=texel[c]*(1-coverage)+e.physical[c]*coverage;}
  const double w=(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy));for(size_t c=0;c<3;++c)result[c]+=texel[c]*w;
 }return result;
}
Rgb resampled(const PaintedMesh&p,double u,double v,int r){const double x=u*r-.5,y=v*r-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));Rgb result{};
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const int px=std::clamp(ix+dx,0,r-1),py=std::clamp(iy+dy,0,r-1);const auto value=physical(p,(px+.5)/r,(py+.5)/r);const double w=(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy));for(size_t c=0;c<3;++c)result[c]+=value[c]*w;}return result;}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;const auto s=Scene::load(argv[1]);const auto&t=s.active();const SculptObject*source=nullptr;for(const auto&o:s.objects())if(o.id!=t.id&&o.painted&&o.painted->has_physical_emission())source=&o;if(!source)throw std::runtime_error("No genuine original HDR source");
 const auto&low=t.painted->mesh();const auto&field=*t.painted->baked()->emission_field;const int r=field.resolution();double residual=0,total=0,at_centres=0;size_t samples=0;std::array<double,2>worst{};std::array<double,3>wanted{},actual{},predicted{};
 for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){double u=(x+.5)/64.,v=(y+.5)/64.;bool inside=false;
 for(size_t tri=0;tri<low.indices.size()/3;++tri){const auto a=low.uv[low.indices[tri*3]],b=low.uv[low.indices[tri*3+1]],c=low.uv[low.indices[tri*3+2]];const double den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);const double p=((b[1]-c[1])*(u-c[0])+(c[0]-b[0])*(v-c[1]))/den,q=((c[1]-a[1])*(u-c[0])+(a[0]-c[0])*(v-c[1]))/den;if(std::min({p,q,1-p-q})>=.08){inside=true;break;}}
 if(!inside)continue;++samples;const auto a=physical(*source->painted,u,v),p=resampled(*source->painted,u,v,r);const auto b=field.sample(float(u),float(v)).resolve({},1);
 for(size_t c=0;c<3;++c){residual=std::max(residual,std::abs(p[c]-b[c]));if(std::abs(a[c]-b[c])>total){total=std::abs(a[c]-b[c]);worst={u,v};wanted=a;for(size_t j=0;j<3;++j)actual[j]=b[j];predicted=p;}}
 }
 for(int y=r/3;y<2*r/3;++y)for(int x=r/3;x<2*r/3;++x){const double u=(x+.5)/r,v=(y+.5)/r;const auto a=physical(*source->painted,u,v);const auto b=field.sample(float(u),float(v)).resolve({},1);for(size_t c=0;c<3;++c)at_centres=std::max(at_centres,std::abs(a[c]-b[c]));}
 std::cout<<std::setprecision(14)<<"HDR_GRID samples="<<samples<<" source="<<source->painted->resolution()<<" target="<<r<<" actual_error="<<total<<" independent_resample_residual="<<residual<<" grid_centre_error="<<at_centres<<" worst="<<worst[0]<<','<<worst[1]<<" wanted="<<wanted[0]<<','<<wanted[1]<<','<<wanted[2]<<" actual="<<actual[0]<<','<<actual[1]<<','<<actual[2]<<" predicted="<<predicted[0]<<','<<predicted[1]<<','<<predicted[2]<<'\n';
 for(int grid:{64,128,256,512,1024,2048}){double error=0;for(int y=0;y<64;++y)for(int x=0;x<64;++x){const auto a=physical(*source->painted,(x+.5)/64.,(y+.5)/64.),b=resampled(*source->painted,(x+.5)/64.,(y+.5)/64.,grid);for(size_t c=0;c<3;++c)error=std::max(error,std::abs(a[c]-b[c]));}std::cout<<"HDR_INDEPENDENT_GRID grid="<<grid<<" original_knot_max="<<error<<'\n';}
 return residual<2e-5&&at_centres<2e-5&&total>.02?0:1;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
