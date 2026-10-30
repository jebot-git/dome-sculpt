// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/sculpt-resolution.hpp"
#include <iostream>
using namespace dome;
int main()try{
 auto require=[](bool v,const char *m){if(!v)throw std::runtime_error(m);std::cout<<"PASS "<<m<<'\n';};
 Volume source(.04f);source.begin_stroke();Brush b;b.radius=.237f;b.blend=0;b.from=b.to={.013f,.017f,-.011f};source.stamp(b);source.end_stroke();source=source.geometry_snapshot();auto owner=source.geometry_owner();auto original=source.voxels();
 SculptResolutionOptions o;o.voxel_size=.02f;float last=-1;unsigned calls=0;
 o.progress=[&](float p){if(!std::isfinite(p)||p<last||p<0||p>=1)throw std::runtime_error("Actual phase/work progress must be monotone and below publication");last=p;++calls;};
 auto result=resample_sculpt(source,o);require(result.replacement&&last==.98f&&calls>10,"Successful quality stages report worker completion without claiming publication");
 calls=0;o.voxel_size=.04f;resample_sculpt(source,o);require(calls==0&&source.geometry_owner()==owner,"Same voxel no-op emits no progress or replacement");
 o.voxel_size=.02f;o.progress=[](float p){if(p>=.8f)throw std::runtime_error("Progress consumer failed");};bool refused=false;try{resample_sculpt(source,o);}catch(const std::runtime_error&){refused=true;}require(refused&&source.voxels()==original&&source.geometry_owner()==owner,"Throwing late progress callback leaves original exact");
 bool cancel=false;o.progress=[&](float p){cancel=p>=.65f;};o.cancelled=[&]{return cancel;};refused=false;try{resample_sculpt(source,o);}catch(const std::runtime_error&){refused=true;}require(refused&&source.voxels()==original&&source.geometry_owner()==owner,"Late phase cancellation leaves original exact");
 return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
