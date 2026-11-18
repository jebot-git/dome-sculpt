// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace advanced_uv_public {
using Json = nlohmann::json;
inline unsigned checks = 0;
void require(bool value, const char *why) { if (!value) throw std::runtime_error(why); ++checks; }
Json oracle(const Scene &before, const Scene &after, SourceNormalUvMethod method) {
  const auto &old = before.object(1), &now = after.object(1);
  advanced_public::advanced_authority(now, method);
  require(old.bake_recipe && now.bake_recipe && old.bake_recipe->encode()==now.bake_recipe->encode() &&
          old.bake_recipe->source.object==2 && before.objects().size()==2 && after.objects().size()==2 &&
          prepared_public::retained_sources(before,1)==prepared_public::retained_sources(after,1),
          "Public UV creation preserves the exact hidden artist and genuine first-Bake recipe");
  const auto a=old.painted->encode().at("mesh"), b=now.painted->encode().at("mesh");
  require(a.at("positions")==b.at("positions") && a.at("indices")==b.at("indices") && a.at("uv")!=b.at("uv"),
          "Public UV rotation changes the actual chart without changing ordered physical geometry");
  double maximum=0;size_t interior=0;
  const auto &mesh=now.painted->mesh();
  for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){
    const std::array<double,2>q{(x+.5)/64.,(y+.5)/64.};
    for(size_t tri=0;tri<mesh.indices.size()/3;++tri){
      const auto p=mesh.uv[mesh.indices[tri*3]],r=mesh.uv[mesh.indices[tri*3+1]],s=mesh.uv[mesh.indices[tri*3+2]];
      const double den=(r[1]-s[1])*(p[0]-s[0])+(s[0]-r[0])*(p[1]-s[1]);
      require(std::isfinite(den)&&std::abs(den)>1e-20,"Public UV creation oracle has a finite independent determinant");
      const double u=((r[1]-s[1])*(q[0]-s[0])+(s[0]-r[0])*(q[1]-s[1]))/den;
      const double v=((s[1]-p[1])*(q[0]-s[0])+(p[0]-s[0])*(q[1]-s[1]))/den;
      require(std::isfinite(u)&&std::isfinite(v)&&std::isfinite(1-u-v),"Public UV creation oracle barycentrics are finite");
      if(std::min({u,v,1-u-v})<.06)continue;
      const std::array<float,3>bary{float(u),float(v),float(1-u-v)};
      for(size_t role=0;role<2;++role){
        const auto wanted=advanced_fixture::decode(old,tri,bary,role),actual=advanced_fixture::decode(now,tri,bary,role);
        finite_sample(wanted);finite_sample(actual);
        for(size_t axis=0;axis<3;++axis)maximum=std::max(maximum,std::abs(wanted[axis]-actual[axis]));
      }
      ++interior;break;
    }
  }
  require(interior>500&&std::isfinite(maximum)&&maximum<.035,"Actual public UV creation preserves both first-Bake normal roles within .035");
  return {{"interior_texels",interior},{"maximum_component_error",maximum},{"maximum_allowed_error",.035},{"finite_samples",true}};
}
Scene expected(const Scene &before, SourceNormalUvMethod method, bool desktop) {
  Document document;document.replace(before.snapshot());require(document.wait_mesh(),"UV-authoring native before publishes");
  document.begin_uv();require(document.wait_uv(),"UV-authoring native topology/validation completes");
  auto view=document.frame().state->uv;
  require(view&&view->topology&&view->valid&&!view->normal_sampling_required&&view->normal_sampling_available&&!view->retain_normal_sampling,
          "Both actual UV representations are publicly selectable from the genuine ordinary first Bake");
  if(method==SourceNormalUvMethod::RetainSource)document.edit_uv(UvAction::RetainNormals);
  const int turns=desktop?6:1;
  for(int n=0;n<turns;++n){document.edit_uv(UvAction::Rotate,desktop?.2617994f:1.570796327f);require(document.wait_uv(),"Native public-toolbar rotation validation completes");}
  document.end_uv(true);require(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty(),"Native public UV Apply publishes without error");
  return document.copy_scene();
}
void emit(const std::filesystem::path &folder) {
  std::filesystem::create_directories(folder);Json cases=Json::object();
  for(bool legacy:{false,true})for(bool live:{false,true})for(auto method:{SourceNormalUvMethod::Rasterize,SourceNormalUvMethod::RetainSource}){
    auto before=advanced_fixture::prepared(legacy,require);advanced_fixture::pose(before,live);before.validate();
    require(before.active().id==1&&before.object(1).painted->normal_projection()->version==1,"Public UV creation starts with actual ordinary first-Bake normals");
    const auto name=std::string(advanced_public::family(method))+(legacy?"-mt0-":"-mt1-")+(live?"posed":"rest");
    const auto out=folder/name;std::filesystem::create_directories(out);before.save(out/"before.dsculpt");Json routes=Json::object();
    for(bool desktop:{false,true}){
      const std::string route=desktop?"desktop":"xr";const auto after=expected(before,method,desktop);
      const auto wrong=expected(before,method==SourceNormalUvMethod::Rasterize?SourceNormalUvMethod::RetainSource:SourceNormalUvMethod::Rasterize,desktop);
      require(after.encode()!=before.encode()&&after.encode()!=wrong.encode(),"Actual public UV method has a distinct non-no-op endpoint and wrong-method counterfactual");
      after.save(out/(route+"-expected.dsculpt"));wrong.save(out/(route+"-wrong-method.dsculpt"));
      routes[route]={{"expected",route+"-expected.dsculpt"},{"wrong_method",route+"-wrong-method.dsculpt"},
                     {"oracle",oracle(before,after,method)},{"turn_count",desktop?6:1},{"turn_radians",desktop?.2617994f:1.570796327f}};
    }
    prepared_public::write_json(out/"manifest.json",{{"version",1},{"kind","advanced-normal-uv-authoring"},{"case",name},
      {"uv_method",advanced_public::method_name(method)},{"before","before.dsculpt"},{"routes",routes},
      {"legacy",legacy},{"live_pose",live},{"source",1},{"original_source",2},{"genuine_first_bake",true},
      {"source_normal_version_before",1},{"source_normal_version_after",method==SourceNormalUvMethod::Rasterize?1:2},
      {"UI_execution",false}});cases[name]=name+"/manifest.json";
  }
  prepared_public::write_json(folder/"manifest.json",{{"version",1},{"kind","advanced-normal-uv-authoring-suite"},{"cases",cases},{"new_targets",0},{"UI_execution",false}});
}
Json verify(const std::filesystem::path &path,const std::string &route,const std::string &phase,const std::filesystem::path &actual_path){
  std::ifstream file(path);Json m;file>>m;require(m.at("version")==1&&m.at("kind")=="advanced-normal-uv-authoring"&&(route=="desktop"||route=="xr"),"UV-authoring verifier requires an explicit valid public capsule");
  const auto before=Scene::load(path.parent_path()/m.at("before").get<std::string>()),actual=Scene::load(actual_path);
  const auto wanted=Scene::load(path.parent_path()/m.at("routes").at(route).at("expected").get<std::string>()),wrong=Scene::load(path.parent_path()/m.at("routes").at(route).at("wrong_method").get<std::string>());
  const auto method=m.at("uv_method")=="Rasterize"?SourceNormalUvMethod::Rasterize:SourceNormalUvMethod::RetainSource;
  require(m.at("uv_method")==advanced_public::method_name(method),"UV-authoring verifier rejects unknown methods");Json normal;
  if(phase=="undone")require(actual.encode()==before.encode(),"Public UV Undo/Cancel restores the exact complete before Scene");
  else{require(phase=="accepted"||phase=="redone"||phase=="reopened","Unknown UV-authoring durable phase");require(actual.encode()==wanted.encode()&&actual.encode()!=wrong.encode(),"Actual public UV creation matches exact native endpoint and rejects wrong-method operation");normal=oracle(before,actual,method);}
  return{{"verified",true},{"case",m.at("case")},{"route",route},{"phase",phase},{"uv_method",advanced_public::method_name(method)},{"normal_oracle",normal},{"checks",checks}};
}
} // namespace advanced_uv_public
