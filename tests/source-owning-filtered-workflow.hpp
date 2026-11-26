// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-target-resolution.hpp"
#include "dome/bake-recipe.hpp"
#include <fstream>
#include <iomanip>

namespace owning_filtered {
inline uint32_t resolution(std::string_view text) {
  size_t end=0;const auto value=std::stoul(std::string(text),&end);
  if(end!=text.size()||value<64||value>2048||(value&(value-1)))
    throw std::invalid_argument("Select an explicit target resolution: 64,128,256,512,1024,2048");
  return uint32_t(value);
}
inline std::vector<bool> bits(std::string_view text,size_t count) {
  if(text.size()!=count||text.find_first_not_of("01")!=std::string_view::npos)
    throw std::invalid_argument("Checked SOURCE case must name every existing binary case axis");
  std::vector<bool> result;for(char c:text)result.push_back(c=='1');return result;
}
inline std::vector<uint8_t> bytes(const std::filesystem::path &path) {
  std::ifstream file(path,std::ios::binary);
  if(!file)throw std::runtime_error("Missing actual checked SOURCE Save");
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
template<class Require>
void select_resolution(Scene &scene,uint64_t source,uint32_t selected,
    const std::filesystem::path &out,Require require) {
  const auto target=scene.active().id;const auto original=scene.encode();
  require(scene.active().painted->resolution()==64&&scene.object(source).painted->resolution()==64,
      "Checked SOURCE starts from the genuine unchanged source64 and target64");
  if(selected==64)return;
  Document doc;doc.replace(scene.snapshot());require(doc.wait_mesh(),"Checked SOURCE resize publishes original input");
  require(doc.frame().state->can_resize_bake_target,"Real native target-resolution editor admits this unpainted destination");
  const auto state=doc.frame().state;const AssetSource authority{state->generation,state->revision,target,state->edit_authority};
  TextureSetEdit edit;edit.action=TextureSetAction::Resolution;edit.set=0;edit.resolution=selected;
  doc.edit_texture_set(edit,authority);const auto path=out/"selected-target-resolution.dsculpt";doc.save_async(path).get();
  require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Explicit SOURCE target resize and ordered Save succeed");
  const auto accepted=doc.copy_scene();require(accepted.active().painted->resolution()==selected&&accepted.object(source).painted->resolution()==64,
      "Only explicitly selected destination resolution changes; real source field remains64");
  auto restored=accepted.snapshot();restored.object(target)=scene.object(target);
  require(restored.encode()==original,"Resize preserves all source/recipe/frame/key/pose/UV/paint data exactly");
  require(Scene::load(path).encode()==accepted.encode(),"Selected-resolution native Save reopens exactly");
  require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==original,"One actual resize Undo restores complete original64 scene");
  require(doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted.encode(),"One actual resize Redo restores complete selected-resolution scene");
  bool stale=false;try{doc.edit_texture_set(edit,authority);}catch(const std::exception&){stale=true;}
  require(stale&&doc.copy_scene().encode()==accepted.encode(),"Stale resolution authority cannot change the accepted input");
  scene=accepted;
}
template<class Require>
void refusal(const Scene &input,uint64_t source,const ObjectBakeSettings &settings,
    const std::filesystem::path &out,Require require) {
  require(settings.normal_filter_quality==1,"Refusal exercises actual checked1, never a legacy fallback");
  const auto original=input.encode();auto scene=input.snapshot();const auto paint=scene.active().painted;
  bool rejected=false;std::string reason;
  try{bake_object(scene,source,settings);}catch(const std::invalid_argument &error){rejected=true;reason=error.what();}
  require(rejected&&reason.find("larger target paint resolution")!=std::string::npos,
      "Explicit under-resolution SOURCE case refuses for genuine filtered-detail quality");
  require(scene.encode()==original&&scene.active().painted==paint,"Native quality refusal preserves entire scene and paint owner");
  Document doc;doc.replace(input.snapshot());require(doc.wait_mesh(),"Refusal worker publishes exact source input");
  const auto path=out/"refusal-prior.dsculpt";doc.save_async(path).get();const auto prior=bytes(path);const auto state=doc.frame().state;
  doc.bake_from_object(source,settings);bool failed=false;try{doc.save_async(path).get();}catch(const std::exception&){failed=true;}
  require(failed&&doc.wait_asset()&&doc.wait_mesh()&&doc.copy_scene().encode()==original&&bytes(path)==prior,
      "Checked SOURCE worker refusal preserves whole scene and immediately ordered prior Save bytes");
  const auto after=doc.frame().state;
  require(after->can_undo==state->can_undo&&after->can_redo==state->can_redo&&after->project_path==state->project_path&&
      after->error.find("larger target paint resolution")!=std::string::npos,"Checked refusal preserves history/path and reports explicit resolution remedy");
}
template<class Expected,class Actual,class Require>
void offcentre(const Scene &scene,uint64_t source,Expected expected,Actual actual,Require require) {
  const auto &target=scene.active();const auto &mesh=target.painted->mesh();const auto r=target.painted->resolution();
  require(target.bake_recipe&&target.bake_recipe->settings.normal_filter_quality==1&&
      target.bake_recipe->encode()["settings"]["normal_filter_quality"]==1,"Actual accepted SOURCE recipe persists checked1");
  const auto maps=target.painted->preview(target.material);
  // Independent renderer-equation decoders supplied by each owning fixture.
  // All current-grid reconstruction cells are visited. Near-corner offsets
  // supplement the production quarter/middle samples and retain measured extrema.
  constexpr std::array<double,5> offsets{.001,.25,.5,.75,.999};
  std::array<double,2> maximum{},angle_max{};
  std::array<std::array<double,3>,2> worst{},angle_position{},angle_wanted{},angle_actual{};uint64_t samples=0;
  for(size_t tri=0;tri<mesh.indices.size()/3;++tri){
    const auto a=mesh.uv[mesh.indices[tri*3]],b=mesh.uv[mesh.indices[tri*3+1]],c=mesh.uv[mesh.indices[tri*3+2]];
    const double det=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);
    require(std::isfinite(det)&&std::abs(det)>1e-20,"Independent checked SOURCE target chart is nonsingular");
    auto barycentric=[&](double x,double y){const double u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/det,
      v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/det;return std::array<double,3>{u,v,1-u-v};};
    const int left=std::max(0,int(std::floor(std::min({a[0],b[0],c[0]})*r-.5))),right=std::min(int(r)-2,int(std::ceil(std::max({a[0],b[0],c[0]})*r-.5))),
      bottom=std::max(0,int(std::floor(std::min({a[1],b[1],c[1]})*r-.5))),top=std::min(int(r)-2,int(std::ceil(std::max({a[1],b[1],c[1]})*r-.5)));
    for(int y=bottom;y<=top;++y)for(int x=left;x<=right;++x){
      bool inside=true;for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto q=barycentric((x+.5+dx)/r,(y+.5+dy)/r);inside&=*std::min_element(q.begin(),q.end())>1e-6;}
      if(!inside)continue;
      for(double oy:offsets)for(double ox:offsets){const auto q=barycentric((x+.5+ox)/r,(y+.5+oy)/r);
        const std::array<float,3> bary{float(q[0]),float(q[1]),float(q[2])};
        for(size_t role=0;role<2;++role){const auto wanted=expected(scene.object(source),tri,bary,role),got=actual(target,tri,bary,role,maps.get());
          double error=0,dot=0,aa=0,bb=0;for(size_t axis=0;axis<3;++axis){
            if(!std::isfinite(wanted[axis])||!std::isfinite(got[axis]))throw std::runtime_error("Nonfinite checked SOURCE independent role sample");
            error=std::max(error,std::abs(wanted[axis]-got[axis]));dot+=wanted[axis]*got[axis];aa+=wanted[axis]*wanted[axis];bb+=got[axis]*got[axis];}
          if(!(aa>1e-20&&bb>1e-20))throw std::runtime_error("Singular checked SOURCE independent role sample");
          const auto angle=std::acos(std::clamp(dot/std::sqrt(aa*bb),-1.,1.));
          if(error>maximum[role]){maximum[role]=error;worst[role]={double(tri),(x+.5+ox)/r,(y+.5+oy)/r};}
          if(angle>angle_max[role]){angle_max[role]=angle;angle_position[role]={double(tri),(x+.5+ox)/r,(y+.5+oy)/r};angle_wanted[role]=wanted;angle_actual[role]=got;}
        }++samples;
      }
    }
  }
  const auto precision=std::cout.precision();std::cout<<std::setprecision(12);
  for(size_t role=0;role<2;++role){std::cout<<"CHECKED_SOURCE_EXTREMUM role="<<role<<" samples="<<samples<<" component="<<maximum[role]<<" angle="<<angle_max[role]
      <<" triangle="<<worst[role][0]<<" uv="<<worst[role][1]<<','<<worst[role][2]
      <<" angle_triangle="<<angle_position[role][0]<<" angle_uv="<<angle_position[role][1]<<','<<angle_position[role][2]
      <<" wanted="<<angle_wanted[role][0]<<','<<angle_wanted[role][1]<<','<<angle_wanted[role][2]
      <<" actual="<<angle_actual[role][0]<<','<<angle_actual[role][1]<<','<<angle_actual[role][2]<<std::endl;
    require(samples>300&&maximum[role]<.035&&angle_max[role]<=.03,"All independent current-grid offcentre role samples retain .035 components and selected .03-radian bound");}
  std::cout.precision(precision);
}
} // namespace owning_filtered
