// SPDX-License-Identifier: GPL-3.0-or-later
#include "resolution-ui-values.hpp"
#include <iostream>
int main(){for(float scale:{.001f,.1f,1.f,1.7f,100.f})for(float voxel:{.00001f,.03f,1.f}){dome::SculptResolutionOptions o;o.voxel_size=voxel;dome::ResolutionUiValues d(voxel,scale,o);if(d.accepted().voxel_size!=voxel)throw std::runtime_error("World-unit no-op changed local voxel bits");for(double t:{0.,.5,1.}){d.slider(t);auto a=d.accepted();if(a.voxel_size<.00001f || a.voxel_size>1)throw std::runtime_error("Invalid mapped resolution");}}std::cout<<"PASS 15 object-scale/local-voxel identity and continuous world-mm mappings\n";}
