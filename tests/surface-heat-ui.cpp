// SPDX-License-Identifier: GPL-3.0-or-later
#include "surface-heat-native-fixture.hpp"
int main(int argc,char**argv)try{
 if(argc==3&&std::string(argv[1])=="--ui-fixture"){
  auto f=native_fixture(true);f.scene.object(1).visible=false;f.scene.object(1).locked=true;f.scene.save(argv[2]);return 0;
 }
 if((argc==3||argc==4)&&std::string(argv[1])=="--inspect"){
  auto scene=Scene::load(argv[2]);const auto&o=scene.active();const bool skin=o.deformation&&o.deformation->skin;double foreign=0,max_error=0;
  if(skin){const auto&source=scene.object(1);const auto&raw=source.painted->mesh();const auto&m=o.painted->mesh();auto preview=o.deformation->preview(m);const auto world=scene.world_matrix(o.id),ref=scene.world_matrix(1);
   if(raw.positions.size()!=m.positions.size())throw std::runtime_error("Actual fixture current/source row correspondence");
   for(size_t v=0;v<m.positions.size();++v){const uint32_t j=raw.positions[v].x<.0015f?0:1;foreign=std::max(foreign,influence(o.deformation->skin->vertices[v],1-j));auto p=m.positions[v];if(o.deformation->morphs&&!o.deformation->morph_weights.empty())p+=o.deformation->morphs->targets[0].delta[v]*o.deformation->morph_weights[0];auto expected=world.point({p.x,p.y,p.z});auto delta=ref.direction({0,0,j==0?.1:-.1});for(size_t k=0;k<3;++k)expected[k]+=delta[k];auto actual=world.point({preview.positions[v].x,preview.positions[v].y,preview.positions[v].z});max_error=std::max(max_error,std::hypot(actual[0]-expected[0],actual[1]-expected[1],actual[2]-expected[2]));}
  }
  bool retained=true;if(argc==4){const auto before=Scene::load(argv[3]);auto copy=scene;copy.object(o.id).deformation=before.object(o.id).deformation;copy.object(o.id).rig_source=before.object(o.id).rig_source;retained=copy.encode()==before.encode();}
  std::cout<<nlohmann::json{{"skinned",skin},{"foreign_weight",foreign},{"max_pose_m",max_error},{"retained_exact",retained},{"scene",scene.encode()}}.dump()<<std::endl;return 0;
 }
 return 2;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
