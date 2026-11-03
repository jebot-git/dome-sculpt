// SPDX-License-Identifier: GPL-3.0-or-later
// Native test fixture and independent CPU-backend expected result. No runtime dependency.
#include "dome/scene.hpp"
#include "dome/sculpt-resolution.hpp"
#include <iostream>
using namespace dome;
int main(int argc,char **argv)try{
 if(argc!=3)return 2;
 if(compute_backend()!="CPU")throw std::runtime_error("Independent oracle requires a native CPU build");
 Volume::Voxels samples;
 for(int z=-21;z<=21;++z)for(int y=-21;y<=21;++y)for(int x=-21;x<=21;++x){float d=length(Vec3{float(x),float(y),float(z)}*.01f)-.167f;if(d<.04f)samples.emplace(Int3{x,y,z},std::max(d,-.04f));}
 auto volume=Volume::from_samples(.01f,std::move(samples));
 if(volume.layers()[0].delta.size()<32768)throw std::runtime_error("Fixture not aggregate eligible");
 Scene scene;scene.active().volume=std::make_shared<Volume>(volume.geometry_snapshot());scene.save(argv[1]);
 SculptResolutionOptions o;o.voxel_size=.02f;auto result=resample_sculpt(volume,o);
 if(!result.replacement||result.replacement->layers()[0].delta.size()>volume.layers()[0].delta.size())throw std::runtime_error("Expected result not aggregate eligible");
 scene.active().volume=std::make_shared<Volume>(std::move(*result.replacement));scene.save(argv[2]);
 std::cout<<"CPU_ORACLE original="<<volume.layers()[0].delta.size()<<" target="<<scene.active().volume->layers()[0].delta.size()<<" source_spacing=.01 target_spacing=.02 max_layer_error="<<result.report.maximum_layer_error<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
