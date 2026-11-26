// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-artist-normal-fixture.hpp"
#include "source-artist-normal-oracle.hpp"
#include "bake-fixture-identity.hpp"
#include "affine-surface.hpp"
using namespace source_artist_fixture;
namespace {
void affine_donor(Scene scene, const std::filesystem::path &path) {
  const auto donor=scene.active().id;
  auto &source=scene.active();
  auto edit=AffineTransform{}.matrix();
  edit[0]=-1.2;edit[1]=.18;edit[4]=.32;edit[5]=.75;edit[10]=1.4;
  edit[12]=1.25;edit[13]=-.35;edit[14]=.22;
  source.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(edit));
  source.visible=false;
  const auto world=scene.world_matrix(donor);
  const auto frame=detail::placement_frame(scene,donor);
  detail::SourceBakeNormalOptions options;options.placement=&frame;
  const auto resolver=detail::source_bake_normals(source,{},options);
  const source_artist_oracle::Oracle oracle(source);
  check(bool(resolver),"The affine donor has an authenticated dual-role normal resolver");
  float maximum=0,wrong_frame=0;
  for(uint32_t triangle=0;triangle<source.painted->mesh().indices.size()/3;++triangle){
    for(const auto weights:{std::array<float,3>{.2f,.3f,.5f},std::array<float,3>{.7f,.12f,.18f}}){
      const auto actual=resolver(triangle,weights);
      for(size_t role=0;role<actual.size();++role){
        const auto expected=oracle.point(triangle,weights,role,world).mapped;
        const auto original=oracle.point(triangle,weights,role).mapped;
        const auto wrong=normalized(source_artist_oracle::normal(world.matrix(),original));
        maximum=std::max(maximum,length(actual[role]-expected));
        wrong_frame=std::max(wrong_frame,length(expected-wrong));
      }
    }
  }
  check(maximum<4e-6f&&wrong_frame>.05f,
        "Independent role sampling verifies transformed corner columns and rejects a transformed final normal");

  auto low=source.deformation?source.deformation->preview(source.painted->mesh()):source.painted->mesh();
  for(auto &p:low.positions){const auto placed=world.point({p.x,p.y,p.z});p={float(placed[0]),float(placed[1]),float(placed[2])};}
  low.normals.assign(low.positions.size(),normalized(source_artist_oracle::normal(world.matrix(),{0,0,1})));
  const auto version=source.material_bindings->material.version();
  const auto target=scene.add("Affine artist Bake target",Volume{});
  scene.active().painted=std::make_shared<PaintedMesh>(std::move(low),64);
  scene.active().mesh_active=true;
  auto binding=std::make_shared<SourceMaterialBindings>();
  binding->material=version==MToonVersion::V0
      ? MToonMaterial::from_vrm0({{"shader","VRM/MToon"}})
      : MToonMaterial::from_vrm1({{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});
  scene.active().material_bindings=std::move(binding);
  const auto before=scene.encode();
  auto input=path;input+=".before.dsculpt";scene.save(input);
  const auto source_before=bake_fixture::isolated_scene(scene,donor).encode();
  Document doc;doc.replace(scene);
  const bool ready=doc.wait_mesh();
  const auto view=doc.frame().state;
  check(ready&&view->can_bake&&std::ranges::find(view->bake_sources,donor)!=view->bake_sources.end(),
        "The shared control offers the hidden affine imported or posed source");
  ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=0;
  settings.front_mm=settings.back_mm=2;settings.prepare_normals=true;
  settings.normal_error=.03f;settings.corresponding_topology=true;
  doc.bake_from_object(donor,settings);doc.save_async(path).get();
  check(doc.wait_asset()&&doc.wait_mesh(),"The affine bake and ordered file worker finish");
  if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);
  const auto after=doc.copy_scene();
  const auto &baked=after.object(target).painted->baked();
  check(baked&&!baked->maps.normal.rgba.empty()&&!baked->maps.mtoon_normal.rgba.empty()&&
        baked->maps.normal.rgba!=baked->maps.mtoon_normal.rgba,
        "The matching worker preserves distinct projected PBR and typed normal planes");
  const auto encoded=after.encode();
  check(bake_fixture::isolated_scene(after,donor).encode()==source_before&&Scene::load(path).encode()==encoded&&
        doc.undo()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.copy_scene().encode()==encoded,
        "Affine Bake preserves the exact immutable source, ordered reopen, Undo and Redo");
}
}
int main(int argc,char **argv)try {
  if(argc!=2)return 2;
  const std::filesystem::path out=argv[1];
  std::filesystem::create_directories(out);
  affine_donor(imported_artist_scene(out),out/"authored-affine.dsculpt");
  for(bool legacy:{false,true})for(bool posed:{false,true}){
    run(legacy,posed,out/(std::to_string(legacy)+std::to_string(posed)+".dsculpt"));
    if(posed)affine_donor(fixture(legacy,posed).scene,out/(legacy?"vrm0-affine.dsculpt":"vrm1-affine.dsculpt"));
  }
  std::cout<<"PASS "<<checks<<" artist source CPU controls\n";
  return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
