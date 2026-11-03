// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool b,const char *m){++checks;if(!b)throw std::runtime_error(m);}
Scene fixture(bool reflected,double offset,bool shifted_rest){
 Mesh mesh;mesh.positions={{-.1f,0,0},{.1f,0,0},{0,.5f,0}};mesh.normals.assign(3,{0,0,1});mesh.uv={{{0,0}},{{1,0}},{{.5f,1}}};mesh.indices={0,1,2};
 Scene scene;scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);scene.active().mesh_active=true;DeformationState d(mesh);
 d.skeleton=Skeleton{{{"root",-1,{{shifted_rest?90.f:0,0,0}},2e-6f},{"child",0,{{0,.2f,0}},.3f}}};
 if(shifted_rest){auto m=AffineTransform{}.float_matrix();m[4]=.3f;m[12]=90;d.skeleton->bones[0].rest_matrix=m;}
 scene.set_deformation(1,d);std::array<double,16> world{reflected?-1.3:1.3,.1,0,0,.35,.8,.05,0,.1,.15,1.2,0,offset,offset,-offset,1};
 scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(world));scene.validate();return scene;
}
}
int main()try{
 for(bool reflected:{false,true})for(bool translated:{false,true})for(bool rest_translation:{false,true}){
  auto scene=fixture(reflected,translated?90:0,rest_translation);const auto before=scene.encode();Document doc;doc.replace(scene);
  check(doc.wait_mesh(),"Metric fixture has an actual visible native mesh inside the authoring workspace");doc.begin_rig_fit();const auto view=doc.frame().state->rig_fit;const auto world=view->world.matrix();const auto native=view->rest_source->skeleton->world();
  for(size_t j=0;j<native.size();++j){double axis[3]{};
   for(size_t row=0;row<3;++row)for(size_t k=0;k<3;++k)axis[row]+=world[k*4+row]*double(native[j][4+k]);
   const double expected=std::hypot(axis[0],axis[1],axis[2])*view->rest_source->skeleton->bones[j].length;
   check(view->bone_lengths[j]>0&&std::abs(double(view->bone_lengths[j])-expected)<=expected*2e-7,"Displayed REST length equals independent world linear local-Y metric, including tiny translated bones");
  }
  check(rig_fit_rest_edits(*view).empty(),"Opening rig controls never invents a rounded bone-length edit");doc.configure_rig_fit(*view);
  check(doc.copy_scene().encode()==before,"Unchanged metric controls remain valid and preserve the complete scene");auto changed=*doc.frame().state->rig_fit;changed.bone_lengths[0]*=2;const auto edits=rig_fit_rest_edits(changed);
  check(edits.size()==1&&edits[0].joint==0&&edits[0].world_length==changed.bone_lengths[0]&&!edits[0].world_position&&!edits[0].parent,"Explicit length change has one exact staged world-distance edit");doc.cancel_rig_fit();check(doc.copy_scene().encode()==before,"Cancel retains exact original bones and paint");
 }
 std::cout<<"PASS "<<checks<<" world rig-length metric checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
