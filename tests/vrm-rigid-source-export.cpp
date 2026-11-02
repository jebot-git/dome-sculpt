// SPDX-License-Identifier: GPL-3.0-or-later
#define main previous_source_export_suite
#include "vrm-source-export.cpp"
#undef main
#include "vrm-rigid-fixtures.hpp"
#include "dome/rig-edit.hpp"
std::vector<Vec3> rigid_positions(const cgltf_node &node) {
  const auto &p=node.mesh->primitives[0];
  auto accessor=attribute(p.attributes,p.attributes_count,cgltf_attribute_type_position);
  Matrix world{};cgltf_node_transform_world(&node,world.data());std::vector<Vec3> result;
  for(size_t v=0;v<accessor->count;++v){auto value=vector(accessor,v);for(size_t t=0;t<p.targets_count;++t){auto delta=attribute(p.targets[t].attributes,p.targets[t].attributes_count,cgltf_attribute_type_position);float weight=node.weights_count?node.weights[t]:node.mesh->weights[t];if(delta)value+=vector(delta,v)*weight;}result.push_back(point(world,value));}return result;
}
int main(int argc,char **argv)try{
  if(argc!=4)return 2;
  auto home=std::filesystem::temp_directory_path()/("dome-rigid-vrm-"+std::to_string(getpid()));std::filesystem::create_directories(home);std::filesystem::create_directories(argv[3]);
  setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
  struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
  for(int model=1;model<=2;++model){
    auto input=home/("source-"+std::to_string(model)+".vrm");auto ids=vrm_source_fixture::with_rigid_accessories(argv[model],input);Parsed original(input);
    ImportAuthoringOptions options;options.paint_resolution=64;auto prepared=prepare_authoring_import(Scene{},input,options);
    check(prepared.objects.size()==22,"Actual nineteen-part source plus three genuine unskinned source primitives import natively");
    auto active=prepared.objects.front();size_t attached=0,fixed=0;
    for(auto id:prepared.objects){auto &o=prepared.scene.object(id);if(o.appearance->source_node==int(ids.moving))attached=id;if(o.appearance->source_node==int(ids.fixed))fixed=id;}
    check(attached && fixed && !prepared.scene.object(attached).deformation->skin && !prepared.scene.object(fixed).deformation,"Rigid morph and plain rigid source parts have no fabricated skin/skeleton");
    for(int mode:{0,1,2}){
      auto scene=prepared.scene;
      if(mode){RigEdit edit;edit.joint=JointAdjustment{scene.object(active).deformation->humanoid->joints.at("head"),{0,0,17},{}};edit_rig(scene,active,edit);auto d=*scene.object(attached).deformation;d.morph_weights={.6f};scene.set_deformation(attached,std::move(d));}
      // Independently modify raw source head quaternion for the known local edit.
      if(mode==1){auto &head=original.data->nodes[ids.head];float z=std::sin(17.f*3.14159265358979323846f/360),w=std::cos(17.f*3.14159265358979323846f/360);float x=head.rotation[0],y=head.rotation[1],qz=head.rotation[2],qw=head.rotation[3];head.rotation[0]=x*w+y*z;head.rotation[1]=y*w-x*z;head.rotation[2]=qz*w+qw*z;head.rotation[3]=qw*w-qz*z;}
      Matrix expected{};cgltf_node_transform_world(original.data->nodes+ids.moving,expected.data());auto actual=scene.world_matrix(attached).float_matrix();for(size_t k=0;k<16;++k)check(std::abs(actual[k]-expected[k])<2e-6f,"Linked native source-rig edit carries a real rigid attachment by the independently evaluated original node graph");
      check(scene.world_matrix(fixed).matrix()==prepared.scene.world_matrix(fixed).matrix(),"Independent rigid source root does not follow an unrelated head joint");
      if(mode==2){
        auto &part=scene.object(attached);
        AffineTransform edit(std::array<double,16>{-1.,.12,0,0,.25,1.1,0,0,0,0,.85,0,.04,-.02,.01,1});
        part.affine_edit=std::make_shared<const NativeAffineEdit>(part.affine_edit->matrix()*edit);
        scene.object(fixed).transform.position.x+=.125f;
      }
      auto restart=home/"restart.dsculpt";scene.save(restart);check(Scene::load(restart).encode()==scene.encode(),"Persisted exact rigid pose placement survives native save/restart");
      auto path=std::filesystem::path(argv[3])/(std::to_string(model)+"-rigid-"+std::to_string(mode)+".vrm");auto surfaces=scene.surfaces();export_vrm(surfaces,path);Parsed output(path);check(cgltf_validate(output.data)==cgltf_result_success && output.data->meshes_count==22,"Mixed actual source skins and unskinned accessories emit valid original-space VRM");
      auto metadata=nlohmann::json::parse(output.data->json,output.data->json+output.data->json_size);
      auto input_metadata=nlohmann::json::parse(original.data->json,original.data->json+original.data->json_size);
      const auto &before_meta=input_metadata["extensions"]["VRM"]["meta"];
      const auto &after_meta=metadata["extensions"]["VRM"]["meta"];
      for(const auto &field:before_meta.items())if(field.key()!="texture")
        check(after_meta.at(field.key())==field.value(),"Mixed rigid export preserves exact original author/license/usage metadata");
      for(size_t output_mesh=0;output_mesh<surfaces.size();++output_mesh)
        if(surfaces[output_mesh].appearance->source_node==int(ids.moving)) {
          size_t matches=0;for(const auto &annotation:metadata["extensions"]["VRM"]["firstPerson"]["meshAnnotations"])
            if(annotation["mesh"].get<size_t>()==output_mesh&&annotation["firstPersonFlag"]=="Both")++matches;
          check(matches==1,"Original rigid first-person mesh annotation maps to exactly its emitted source mesh identity");
        }
      size_t rigid=0,skinned=0;
      for(size_t n=0;n<output.data->nodes_count;++n){const auto &node=output.data->nodes[n];if(!node.mesh)continue;const auto &native=surfaces.at(size_t(node.mesh-output.data->meshes));auto values=node.skin?evaluated(node):rigid_positions(node);if(node.skin){++skinned;exact_original_binds(native,node,*original.data);}else{++rigid;check(!attribute(node.mesh->primitives[0].attributes,node.mesh->primitives[0].attributes_count,cgltf_attribute_type_joints),"Rigid output has no dummy skin/joint attributes");}auto mesh=native.preview_mesh();auto world=native.world_matrix();for(size_t c=0;c<mesh.indices.size();++c){auto p=mesh.positions.at(mesh.indices[c]);auto q=world.point({p.x,p.y,p.z});auto value=values.at(cgltf_accessor_read_index(node.mesh->primitives[0].indices,c));check(length(value-Vec3{float(q[0]),float(q[1]),float(q[2])})<2e-6f,"Every ordered emitted mixed skin/morph/rigid world corner matches native preview at unchanged 2um tolerance");}}
      check(rigid==3 && skinned==19,"All nineteen real skinned parts and all three unskinned accessories retain distinct ownership");
      if(mode){auto preserved=scene;scene.erase(active);check(scene.world_matrix(attached).matrix()==preserved.world_matrix(attached).matrix(),"Deleting a skin owner cannot snap a rigid attachment whose exact world placement was persisted");}
    }
    {
      auto rejected = [&](Scene &candidate, RigEdit edit, const char *text,
                          const std::function<bool()> &cancel = {}) {
        auto previous=candidate.encode();bool refused=false;
        try {edit_rig(candidate,active,edit,cancel);} catch(const std::exception &e) {
          refused=std::string_view(e.what()).find(text)!=std::string_view::npos;
        }
        check(refused && candidate.encode()==previous,
              "Rejected/cancelled linked source-rig preparation is atomic with exact source, paint and rigid placement");
      };
      RigEdit edit;edit.joint=JointAdjustment{
        prepared.scene.object(active).deformation->humanoid->joints.at("head"),{0,0,7},{}};
      auto locked=prepared.scene;locked.object(attached).locked=true;
      rejected(locked,edit,"locked");
      auto reference=prepared.scene;auto other_reference=prepared.objects[1];
      auto rig=*reference.object(other_reference).deformation;
      auto head=rig.humanoid->joints.at("head");
      rig.skeleton->bones[head].rest.rotation=Quat{std::cos(.07f),0,0,std::sin(.07f)};
      reference.set_deformation(other_reference,std::move(rig));
      rejected(reference,edit,"rest axes differ");
      auto missing=prepared.scene.surfaces();
      std::erase_if(missing,[&](const auto &surface){return surface.appearance->source_node==int(ids.instance);});
      auto protected_output=home/"missing-instance.vrm";
      {std::ofstream f(protected_output);f<<"preserved output";}
      bool missing_refused=false;
      try{export_vrm(missing,protected_output);}catch(const std::invalid_argument&e){missing_refused=std::string_view(e.what()).find("missing original mesh instances")!=std::string_view::npos;}
      std::ifstream retained(protected_output);std::string content{std::istreambuf_iterator<char>(retained),{}};
      check(missing_refused&&content=="preserved output","Missing one shared-mesh source instance refuses atomically instead of passing a unique-primitive count check");
      auto changed=[](DeformationState d,float angle) {
        for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);
        auto head=d.humanoid->joints.at("head");
        d.pose[head].rotation=(d.pose[head].rotation*Quat{std::cos(angle),0,0,std::sin(angle)}).unit();
        return d;
      };
      auto unique=prepared.scene;
      unique.set_deformation(active,changed(*unique.object(active).deformation,.07f));
      rejected(unique,edit,"unsynchronised authored pose");
      auto conflict=unique;auto other=prepared.objects[1];
      conflict.set_deformation(other,changed(*conflict.object(other).deformation,.14f));
      rejected(conflict,edit,"conflicting authored poses");
      size_t total=0;auto measured=prepared.scene;
      edit_rig(measured,active,edit,[&]{++total;return false;});
      for(size_t boundary:{size_t(1),total/2,total}) {
        size_t at=0;auto cancelled=prepared.scene;
        rejected(cancelled,edit,"cancelled",[&]{return ++at==boundary;});
      }
      auto capsule=capture_source_placement(measured,attached);
      check(capsule && capsule->world_matrix().matrix()==measured.world_matrix(attached).matrix(),
            "Existing retained placement capsule captures the complete durable posed rigid world, without a live skin dependency");
    }
  }
  std::cout<<checks<<" mixed rigid/source VRM checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
