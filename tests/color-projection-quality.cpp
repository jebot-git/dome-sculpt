// SPDX-License-Identifier: GPL-3.0-or-later
#include "color-projection-quality.hpp"
#include <iostream>
using namespace dome;
namespace {
int checks=0;
void check(bool value,const char *text){if(!value)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh quad(){Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};m.normals.assign(4,{0,0,1});m.indices={0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};return m;}
}
int main()try{
 auto mesh=quad();std::vector<std::array<float,4>> colors{{0,0,1,1},{1,0,0,1},{1,1,0,1},{0,1,1,1}};
 Image image{64,64,std::vector<uint8_t>(64*64*4)};
 for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){const size_t at=(y*64+x)*4;image.rgba[at]=uint8_t(std::lround((x+.5)/64*255));image.rgba[at+1]=uint8_t(std::lround((y+.5)/64*255));image.rgba[at+2]=255-image.rgba[at];image.rgba[at+3]=255;}
 auto bound=detail::color_projection_bound(mesh,colors,image);
 check(bound.tested_cells>4096 && bound.intersecting_cells>4096,"Every triangle/bilinear sampling cell is tested, including clamp borders");
 check(std::abs(bound.maximum_error[0]-2.0/255)<2e-6 && bound.maximum_error[3]<2e-6,"Analytic clamp-border error and exact constant alpha are bounded independently of probe density");
 // On this triangle every vertex error is zero, but the hypotenuse's
 // quadratic error has its maximum .25 at the midpoint.
 Mesh triangle;triangle.positions={{0,0,0},{1,0,0},{0,1,0}};triangle.normals.assign(3,{0,0,1});triangle.indices={0,1,2};triangle.uv={{{.25f,.25f}},{{.75f,.25f}},{{.25f,.75f}}};
 Image corner{2,2,std::vector<uint8_t>(16)};for(int c=0;c<4;++c)corner.rgba[12+c]=255;
 auto curved=detail::color_projection_bound(triangle,std::vector<std::array<float,4>>(3,{0,0,0,0}),corner);
 check(std::abs(curved.maximum_error[0]-.25)<2e-6 && std::abs(curved.maximum_error[3]-.25)<2e-6,"Quadratic edge stationary points catch a maximum missed by all triangle vertices");
 Image white{1,1,{255,255,255,255}};auto lost=detail::color_projection_bound(mesh,std::vector<std::array<float,4>>(4,{0,0,0,0}),white);
 check(lost.maximum_error==std::array<double,4>{1,1,1,1},"Zero RGB and alpha against white correctly report complete unrepresentability");
 check(rejects([&]{detail::color_projection_bound(mesh,colors,image,1);}),"A work budget bounds analysis without unbounded triangle/texel iteration");
 check(rejects([&]{detail::color_projection_bound(mesh,colors,image,10000,[]{return true;});}),"Cancellation fails before a partial quality receipt can be published");
 auto bad=mesh;bad.uv[1]=bad.uv[0];check(rejects([&]{detail::color_projection_bound(bad,colors,image);}),"Degenerate destination UV faces require an explicit new atlas");
 auto absent=colors;absent.pop_back();check(rejects([&]{detail::color_projection_bound(mesh,absent,image);}),"An incomplete color identity domain cannot produce a quality claim");
 std::cout<<"Color projection quality checks "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
