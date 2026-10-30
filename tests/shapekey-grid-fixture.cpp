// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
int main(int argc,char**argv){
 if(argc!=2)return 2;dome::Mesh mesh;constexpr unsigned n=17;
 for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x){mesh.positions.push_back({(float(x)/(n-1)-.5f)*1.6f,(float(y)/(n-1)-.5f)*1.6f,0});mesh.normals.push_back({0,0,1});mesh.uv.push_back({float(x)/(n-1),float(y)/(n-1)});}
 for(unsigned y=0;y<n-1;++y)for(unsigned x=0;x<n-1;++x){auto a=y*n+x,b=a+1,c=a+n,d=c+1;mesh.indices.insert(mesh.indices.end(),{a,b,d,a,d,c});}
 dome::Scene scene;scene.active().mesh_active=true;scene.active().painted=std::make_shared<dome::PaintedMesh>(mesh,64);scene.save(argv[1]);
}
