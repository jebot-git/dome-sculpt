// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/vrm.hpp"
#include "cgltf.h"
#include "dome/creation.hpp"
#include "dome/import.hpp"
#include "dome/source-maps.hpp"
#include "dome/material-bindings.hpp"
#include <fstream>
#include <iostream>
#include <numeric>
#include <set>
#include <png.h>
#include <unistd.h>
using namespace dome;
namespace {
void check(bool ok, const char *what) {
  if (!ok)
    throw std::runtime_error(what);
  std::cout << "PASS " << what << '\n';
}
template <class F> bool rejects(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &e) {
    std::cout << "REJECT " << e.what() << '\n';
    return true;
  }
}
nlohmann::json fixture() {
  using J = nlohmann::json;
  J bones = J::array();
  for (const char *role :
       {"hips", "spine", "head", "leftUpperArm", "leftLowerArm", "leftHand",
        "rightUpperArm", "rightLowerArm", "rightHand", "leftUpperLeg",
        "leftLowerLeg", "leftFoot", "rightUpperLeg", "rightLowerLeg",
        "rightFoot"})
    bones.push_back({{"bone", role}, {"node", bones.size()}});
  return {{"specVersion", "0.0"},
          {"meta",
           {{"title", "Fixture"},
            {"author", "CC0 author"},
            {"licenseName", "CC0"},
            {"texture", 0}}},
          {"humanoid", {{"humanBones", bones}}},
          {"firstPerson",
           {{"firstPersonBone", 2},
            {"meshAnnotations", {{{"mesh", 0}, {"firstPersonFlag", "Both"}}}}}},
          {"blendShapeMaster",
           {{"blendShapeGroups",
             {{{"name", "Smile"},
               {"presetName", "joy"},
               {"isBinary", false},
               {"binds", {{{"mesh", 0}, {"index", 1}, {"weight", 100}}}}}}}}},
          {"secondaryAnimation",
           {{"boneGroups",
             {{{"center", 0},
               {"bones", {1, 2}},
               {"colliderGroups", {0}},
               {"stiffiness", .5},
               {"dragForce", .2}}}},
            {"colliderGroups",
             {{{"node", 1},
               {"colliders",
                {{{"offset", {{"x", 0}, {"y", 0}, {"z", 0}}},
                  {"radius", .1}}}}}}}}},
          {"materialProperties",
           {{{"name", "Mat"},
             {"shader", "VRM/MToon"},
             {"textureProperties", {{"_MainTex", 0}}}}}}};
}
Vec3 transformed(const float *m, Vec3 p) {
  return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
          m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
          m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}
void compare_skin(cgltf_data *data, const std::vector<ObjectSurface> &objects) {
  for (size_t i = 0; i < objects.size(); ++i) {
    const auto &object = objects[i];
    auto expected = object.preview_mesh();
    auto &primitive = data->meshes[i].primitives[0];
    cgltf_skin *skin = nullptr;
    for (size_t n = 0; n < data->nodes_count; ++n)
      if (data->nodes[n].mesh == &data->meshes[i])
        skin = data->nodes[n].skin;
    if (!skin)
      throw std::runtime_error("Missing independent skin");
    for (size_t v : {size_t(0), object.mesh.positions.size() / 2,
                     object.mesh.positions.size() - 1}) {
      auto positions =
          cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
      float authored[3];
      if (!positions || !cgltf_accessor_read_float(positions, v, authored, 3))
        throw std::runtime_error("Missing independent source-basis position");
      Vec3 p{authored[0], authored[1], authored[2]};
      for (size_t t = 0; t < data->meshes[i].weights_count; ++t)
        if (data->meshes[i].weights[t]) {
          if (t >= primitive.targets_count)
            throw std::runtime_error("Missing independent morph target");
          cgltf_accessor *delta = nullptr;
          auto &target = primitive.targets[t];
          for (size_t attribute = 0; attribute < target.attributes_count;
               ++attribute)
            if (target.attributes[attribute].type ==
                cgltf_attribute_type_position)
              delta = target.attributes[attribute].data;
          float value[3];
          if (!delta || !cgltf_accessor_read_float(delta, v, value, 3))
            throw std::runtime_error("Invalid independent morph accessor");
          p += Vec3{value[0], value[1], value[2]} * data->meshes[i].weights[t];
        }
      Vec3 weighted{};
      for (size_t set = 0;; ++set) {
        auto joints = cgltf_find_accessor(
            &primitive, cgltf_attribute_type_joints, int(set));
        auto weights = cgltf_find_accessor(
            &primitive, cgltf_attribute_type_weights, int(set));
        if (!joints)
          break;
        float slots[4], values[4];
        if (!weights || !cgltf_accessor_read_float(joints, v, slots, 4) ||
            !cgltf_accessor_read_float(weights, v, values, 4))
          throw std::runtime_error("Invalid independent influence sets");
        for (size_t k = 0; k < 4; ++k)
          if (values[k]) {
            size_t joint = size_t(slots[k]);
            float ibm[16], world[16];
            if (joint >= skin->joints_count ||
                !cgltf_accessor_read_float(skin->inverse_bind_matrices, joint,
                                           ibm, 16))
              throw std::runtime_error("Invalid independent inverse bind");
            cgltf_node_transform_world(skin->joints[joint], world);
            weighted += transformed(world, transformed(ibm, p)) * values[k];
          }
      }
      auto reference = expected.positions[v];
      if (object.appearance && object.appearance->source_basis_quaternion)
        reference =
            object.appearance->source_basis_quaternion->inverse().rotate(
                reference);
      if (length(weighted - reference) > 3e-4f)
        throw std::runtime_error("Independent real-avatar joint/IBM evaluation "
                                 "differs from native preview");
    }
  }
}
uint32_t word(const std::vector<uint8_t> &b, size_t p) {
  return uint32_t(b.at(p)) | uint32_t(b.at(p + 1)) << 8 |
         uint32_t(b.at(p + 2)) << 16 | uint32_t(b.at(p + 3)) << 24;
}
std::vector<uint8_t> read(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw std::runtime_error("Cannot read fixture");
  return {std::istreambuf_iterator<char>(f), {}};
}
std::pair<nlohmann::json,std::vector<uint8_t>> container(const std::filesystem::path &p) {
  auto bytes=read(p);auto json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+word(bytes,12));
  return {std::move(json),std::move(bytes)};
}
Image texture_png(const nlohmann::json &json,const std::vector<uint8_t> &bytes,int texture) {
  const auto &image=json.at("images").at(json.at("textures").at(size_t(texture)).at("source").get<size_t>());
  const auto &view=json.at("bufferViews").at(image.at("bufferView").get<size_t>());
  size_t offset=28+word(bytes,12)+view.value("byteOffset",size_t{}),size=view.at("byteLength");
  if(offset>bytes.size() || size>bytes.size()-offset)throw std::runtime_error("Independent texture range is outside the VRM");
  png_image decoded{};decoded.version=PNG_IMAGE_VERSION;
  if(!png_image_begin_read_from_memory(&decoded,bytes.data()+offset,size))throw std::runtime_error("Independent PNG decoder rejects painted VRM texture");
  decoded.format=PNG_FORMAT_RGBA;Image result{decoded.width,decoded.height,std::vector<uint8_t>(PNG_IMAGE_SIZE(decoded))};
  bool ok=png_image_finish_read(&decoded,nullptr,result.rgba.data(),0,nullptr);png_image_free(&decoded);
  if(!ok)throw std::runtime_error("Independent PNG decoder cannot read painted VRM pixels");return result;
}
std::vector<ObjectSurface> source_metadata(std::vector<ObjectSurface> objects,
    const std::function<void(nlohmann::json&)> &edit) {
  auto original=objects.front().appearance->original_bytes;
  auto json=nlohmann::json::parse(original->begin()+20,original->begin()+20+word(*original,12));edit(json);
  auto encoded=json.dump();while(encoded.size()%4)encoded.push_back(' ');
  auto bytes=std::make_shared<std::vector<uint8_t>>();
  auto append=[&](uint32_t value){for(int i=0;i<4;++i)bytes->push_back(uint8_t(value>>(i*8)));};
  auto tail=20+word(*original,12);
  append(0x46546c67);append(2);append(uint32_t(20+encoded.size()+original->size()-tail));
  append(uint32_t(encoded.size()));append(0x4e4f534a);
  bytes->insert(bytes->end(),encoded.begin(),encoded.end());bytes->insert(bytes->end(),original->begin()+tail,original->end());
  auto vrm=std::make_shared<const std::string>(json.at("extensions").at("VRM").dump());
  for(auto &object:objects) {
    auto appearance=std::make_shared<CreatorAppearance>(*object.appearance);
    appearance->original_bytes=bytes;appearance->vrm_json=vrm;object.appearance=std::move(appearance);
  }
  return objects;
}
std::shared_ptr<SourceMaterialBindings> material_binding(const ObjectSurface &object) {
  const auto &a=*object.appearance;auto result=std::make_shared<SourceMaterialBindings>();
  auto vrm=nlohmann::json::parse(*a.vrm_json);
  result->material=MToonMaterial::from_vrm0(vrm.at("materialProperties").at(size_t(a.source_material)),a.asset->textures.size());
  for(size_t role=0;role<result->roles.size();++role)if(result->material.textures[role].index>=0) {
    const auto &source=a.asset->textures.at(size_t(result->material.textures[role].index));auto &view=result->roles[role];
    view.image=a.decoded_images->at(size_t(source.image));view.wrap_s=source.wrap_s;view.wrap_t=source.wrap_t;
    view.min_filter=source.min_filter;view.mag_filter=source.mag_filter;
  }
  const auto &native=a.asset->materials.at(size_t(a.material));
  result->native_reference.color=native.color;result->native_reference.emission=native.emissive;
  result->native_reference.alpha=native.alpha==ImportedMaterial::AlphaMode::Opaque?int(Material::AlphaMode::Opaque):
      native.alpha==ImportedMaterial::AlphaMode::Mask?int(Material::AlphaMode::Mask):int(Material::AlphaMode::Blend);
  result->native_reference.alpha_cutoff=native.alpha_cutoff;result->native_reference.double_sided=native.double_sided;
  return result;
}
} // namespace
int main(int argc, char **argv) try {
  auto source = fixture();
  source["meta"]["customLicenseNote"] = "Keep exactly";
  auto metadata = Vrm0Metadata::parse(source, 15, 1, 1, 1);
  check(metadata.source() == source, "VRM0 native metadata retains complete "
                                     "source including custom license fields");
  Vrm0IdentityMap map;
  map.nodes.resize(15);
  std::iota(map.nodes.begin(), map.nodes.end(), 100);
  map.textures = {40};
  map.meshes = {{5, 7}};
  map.morph_targets = {{5, {0, 1}}, {7, {1, 0}}};
  auto result = metadata.remap(map);
  check(result["humanoid"]["humanBones"][0]["node"] == 100 &&
            result["firstPerson"]["firstPersonBone"] == 102 &&
            result["meta"]["texture"] == 40,
        "Humanoid, first-person and thumbnail use exact emitted identities");
  auto binds = result["blendShapeMaster"]["blendShapeGroups"][0]["binds"];
  check(binds.size() == 2 && binds[0]["mesh"] == 5 && binds[0]["index"] == 1 &&
            binds[1]["mesh"] == 7 && binds[1]["index"] == 0,
        "Split source meshes expand expressions with independently reordered "
        "morph identities");
  auto inserted = map;
  inserted.morph_targets[5] = {1, 2};
  inserted.morph_targets[7] = {0, 1};
  auto independently = metadata.remap(inserted);
  const auto &independent_binds =
      independently["blendShapeMaster"]["blendShapeGroups"][0]["binds"];
  check(independent_binds[0]["index"] == 2 && independent_binds[1]["index"] == 1 &&
            metadata.source() == source,
        "Each emitted mesh remaps the immutable original expression target even after native-only target insertion");
  check(result["firstPerson"]["meshAnnotations"].size() == 2 &&
            result["secondaryAnimation"]["boneGroups"][0]["bones"][1] == 102 &&
            result["secondaryAnimation"]["colliderGroups"][0]["node"] == 101,
        "Mesh annotations, spring roots/centers and collider nodes remap "
        "explicitly");
  check(result["materialProperties"][0]["textureProperties"]["_MainTex"] ==
                40 &&
            result["meta"]["customLicenseNote"] == "Keep exactly",
        "MToon texture and full licensing provenance survive remapping");
  auto broken = source;
  broken["humanoid"]["humanBones"][0]["node"] = 1.5;
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "Fractional node references reject");
  broken = source;
  broken["humanoid"]["humanBones"].erase(0);
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "Incomplete humanoid mappings reject");
  broken = source;
  broken["meta"]["licenseName"] = "Unspecified";
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "Unknown license enum rejects without changing source permissions");
  broken = source;
  broken["secondaryAnimation"]["boneGroups"][0]["colliderGroups"] = {5};
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "Spring references outside original collider tables reject");
  broken = source;
  broken["specVersion"] = "1.0";
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "VRM1 is never silently interpreted as VRM0");
  auto missing = map;
  missing.nodes[2] = -1;
  check(rejects([&] { metadata.remap(missing); }),
        "Missing source joint identities reject instead of inferring names");
  missing = map;
  missing.morph_targets[7] = {0, -1};
  check(rejects([&] { metadata.remap(missing); }),
        "Missing source morph identities reject even when target counts match");
  for (int arg = 1; arg < argc; ++arg) {
    std::string gender = argv[arg];
    auto directory = std::filesystem::path("artifacts/vrm-export-" + gender);
    std::filesystem::create_directories(directory);
    Scene scene;
    auto group = append_humanoid(
        scene, humanoid_baseline_defaults("vroid-beta-" + gender), "Avatar", {},
        64);
    auto native = directory / "source.dsculpt";
    scene.save(native);
    auto reopened = Scene::load(native);
    auto objects = reopened.preview_surfaces();
    std::erase_if(objects, [&](auto &o) {
      return o.groups.empty() || o.groups.back().id != group;
    });
    check(objects.size() == 19, "Actual CC0 avatar restarts from native Scene "
                                "with all19 source primitives");
    auto path = directory / "avatar.vrm";
    auto report = export_vrm(objects, path);
    std::cout << "NORMAL_POLICY " << gender
              << " recomputed=" << report.normal_targets_recomputed << '\n';
    cgltf_options options{};
    cgltf_data *raw = nullptr;
    check(cgltf_parse_file(&options, path.c_str(), &raw) ==
              cgltf_result_success,
          "Independent cgltf parser accepts emitted VRM container");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> parsed(raw, cgltf_free);
    check(cgltf_load_buffers(&options, raw, path.c_str()) ==
                  cgltf_result_success &&
              cgltf_validate(raw) == cgltf_result_success,
          "Independent cgltf validates all avatar accessors, images and "
          "hierarchy");
    check(raw->skins_count == 1 && raw->meshes_count == 19 &&
              raw->skins[0].joints_count ==
                  objects[0].deformation->skeleton->bones.size(),
          "All19 avatar meshes bind one actual shared source-derived rig");
    compare_skin(raw, objects);
    check(true, "Independent real-avatar skin/IBM evaluation matches native "
                "preview on all19 parts");
    auto bytes = read(path);
    auto dormant=objects;dormant[0].source_dormant=true;dormant[0].appearance.reset();dormant[0].deformation.reset();
    check(rejects([&]{export_vrm(dormant,path);}) && read(path)==bytes,
          "Dormant source geometry cannot replace an existing VRM after active metadata is hidden");
    auto json = nlohmann::json::parse(bytes.begin() + 20,
                                      bytes.begin() + 20 + word(bytes, 12));
    auto vrm = json.at("extensions").at("VRM");
    auto original = nlohmann::json::parse(*objects[0].appearance->vrm_json);
    auto expected_meta = original.at("meta");
    expected_meta["texture"] = vrm.at("meta").at("texture");
    check(vrm.at("meta") == expected_meta,
          "Full original author, license, permissions and thumbnail metadata "
          "are retained");
    check(vrm.at("secondaryAnimation").at("boneGroups").size() ==
                  original.at("secondaryAnimation").at("boneGroups").size() &&
              vrm.at("firstPerson").at("lookAtHorizontalInner") ==
                  original.at("firstPerson").at("lookAtHorizontalInner"),
          "Spring groups and authored look-at curves survive actual source "
          "export");
    const auto &appearance = *objects[0].appearance;
    float fit_scale = 1;
    const auto &bones = *objects[0].deformation->skeleton;
    for (size_t joint = 0; joint < bones.bones.size(); ++joint) {
      auto original_node = appearance.source_joint_nodes[joint];
      auto t = appearance.asset->nodes[original_node].translation;
      float distance = length(Vec3{t[0], t[1], t[2]});
      if (bones.bones[joint].parent >= 0 && distance > .01f) {
        fit_scale = length(bones.bones[joint].rest.translation) / distance;
        break;
      }
    }
    auto original_offset =
        original.at("firstPerson").at("firstPersonBoneOffset");
    for (const char *axis : {"x", "y", "z"})
      check(std::abs(vrm.at("firstPerson")
                         .at("firstPersonBoneOffset")
                         .at(axis)
                         .get<float>() -
                     original_offset.at(axis).get<float>() * fit_scale) < 2e-6f,
            "First-person bone-local offset follows independently measured "
            "uniform source scale");
    if (!original["secondaryAnimation"]["colliderGroups"].empty()) {
      float old_radius = original.at("secondaryAnimation")
                             .at("colliderGroups")[0]
                             .at("colliders")[0]
                             .at("radius");
      check(std::abs(vrm.at("secondaryAnimation")
                         .at("colliderGroups")[0]
                         .at("colliders")[0]
                         .at("radius")
                         .get<float>() -
                     old_radius * fit_scale) < 2e-6f,
            "Collider radius scales with actual native/source joint distances");
    } else
      check(vrm["secondaryAnimation"]["colliderGroups"].empty(),
            "Avatar without authored colliders does not invent collision "
            "metadata");
    auto native_worlds = objects[0].deformation->skeleton->world();
    bool collider_frames = true;
    for (size_t g = 0;
         g < original["secondaryAnimation"]["colliderGroups"].size(); ++g) {
      auto source_node =
          original["secondaryAnimation"]["colliderGroups"][g]["node"]
              .get<uint32_t>();
      auto found = std::find(appearance.source_joint_nodes.begin(),
                             appearance.source_joint_nodes.end(), source_node);
      if (found == appearance.source_joint_nodes.end())
        throw std::runtime_error(
            "Source collider lacks explicit joint provenance");
      size_t joint = size_t(found - appearance.source_joint_nodes.begin());
      auto output_node =
          vrm["secondaryAnimation"]["colliderGroups"][g]["node"].get<size_t>();
      float output_world[16];
      cgltf_node_transform_world(&raw->nodes[output_node], output_world);
      auto &colliders =
          original["secondaryAnimation"]["colliderGroups"][g]["colliders"];
      for (size_t c = 0; c < colliders.size(); ++c) {
        auto offset = colliders[c]["offset"];
        Vec3 local{offset["x"], offset["y"], offset["z"]};
        auto native_local =
            appearance.source_basis_quaternion->rotate(local * fit_scale);
        auto reference = appearance.source_basis_quaternion->inverse().rotate(
            transformed(native_worlds[joint].data(), native_local));
        auto exported = vrm["secondaryAnimation"]["colliderGroups"][g]
                           ["colliders"][c]["offset"];
        auto actual = transformed(
            output_world, {exported["x"], exported["y"], exported["z"]});
        collider_frames &= length(actual - reference) < 2e-5f;
      }
    }
    check(collider_frames, "Independent collider world centers agree after "
                           "basis restoration and fitted local offsets");
    check(report.normal_targets_recomputed == 0,
          "Neutral source basis conversion preserves authored morph normals "
          "without geometric replacement");
    for (size_t i = 0; i < objects.size(); ++i) {
      auto normals = cgltf_find_accessor(&raw->meshes[i].primitives[0],
                                         cgltf_attribute_type_normal, 0);
      float normal[3];
      check(normals && cgltf_accessor_read_float(normals, 0, normal, 3),
            "Independent source-basis normal accessor is readable");
      auto expected =
          objects[i].appearance->source_basis_quaternion->inverse().rotate(
              objects[i].mesh.normals[0]);
      check(length(Vec3{normal[0], normal[1], normal[2]} - expected) < 1e-6f,
            "Exported normal follows explicit inverse source basis");
    }
    size_t morph_normals = 0;
    for (auto &m : json.at("meshes"))
      for (auto &p : m.at("primitives"))
        for (auto &t : p.value("targets", nlohmann::json::array()))
          morph_normals += t.contains("NORMAL");
    size_t expected_normals = 0;
    for (auto &p : objects[0].appearance->asset->primitives)
      for (auto &t : p.morph_targets)
        expected_normals += !t.normals.empty();
    check(morph_normals == expected_normals,
          "Every source NORMAL morph channel survives native compacted vertex "
          "export");
    auto raw_objects = objects;
    for (auto &part : raw_objects)
      part.textures = reopened.object(part.id).painted->preview(part.material);
    auto raw_path = directory / "raw-avatar.vrm";
    auto raw_report = export_vrm(raw_objects, raw_path);
    auto raw_bytes = read(raw_path);
    auto raw_json = nlohmann::json::parse(
        raw_bytes.begin() + 20, raw_bytes.begin() + 20 + word(raw_bytes, 12));
    bool source_colors = true;
    for (size_t i = 0; i < raw_objects.size(); ++i) {
      auto source_material = raw_objects[i].appearance->source_material;
      auto output_material=raw_json["meshes"][i]["primitives"][0]["material"].get<size_t>();
      source_colors &= raw_json["extensions"]["VRM"]["materialProperties"][output_material]
                               ["vectorProperties"]["_Color"] ==
                       original["materialProperties"][source_material]
                               ["vectorProperties"]["_Color"];
    }
    check(source_colors && raw_report.normal_targets_recomputed == 0,
          "Raw-source VRM preserves original gamma MToon colors and authored "
          "morph normals exactly");
    std::map<int,int> material_identity;
    std::set<std::string> material_names;
    for(const auto &m:raw_json.at("materials"))material_names.insert(m.at("name"));
    for(size_t i=0;i<objects.size();++i) {
      auto original_id=objects[i].appearance->source_material;
      auto output_id=raw_json["meshes"][i]["primitives"][0]["material"].get<int>();
      auto [found,inserted]=material_identity.emplace(original_id,output_id);
      if(!inserted && found->second!=output_id)throw std::runtime_error("Identical source material state was duplicated across raw avatar parts");
    }
    check(raw_report.materials==original["materialProperties"].size() &&
          material_names.size()==raw_json["materials"].size(),
          "Raw material states deduplicate within exact original IDs and have unique emitted names");
    size_t shared_part=SIZE_MAX,shared_coverage=0;
    for(size_t i=0;i<objects.size();++i)for(size_t k=0;k<objects.size();++k)
      if(i!=k && objects[i].appearance->source_material==objects[k].appearance->source_material) {
        auto covered=reopened.object(objects[i].id).painted->surface_diagnostics().covered_texels;
        if(covered>shared_coverage){shared_coverage=covered;shared_part=i;}
      }
    if(shared_part==SIZE_MAX)throw std::runtime_error("Actual avatar fixture lacks a shared source material");
    auto painted_scene=reopened.snapshot();painted_scene.select(objects[shared_part].id);
    auto painting=painted_scene.active().painted;
    PaintSettings ink;ink.color={0,1,0,1};ink.front_only=false;
    Brush brush;brush.radius=5;brush.from=brush.to=painting->mesh().positions.front();
    painting->begin_stroke();for(int sample=0;sample<16;++sample)painting->stamp(brush,ink);painting->end_stroke();
    auto painted_native=painted_scene.encode();auto painted_objects=painted_scene.preview_surfaces();
    std::erase_if(painted_objects,[&](const auto &o){return o.groups.empty()||o.groups.back().id!=group;});
    auto painted_path=directory/"painted-avatar.vrm";VrmExportOptions painted_options;painted_options.paint_resolution=64;
    auto painted_report=export_vrm(painted_objects,painted_path,painted_options);
    auto [painted_json,painted_bytes]=container(painted_path);
    auto painted_material=painted_json["meshes"][shared_part]["primitives"][0]["material"].get<size_t>();
    auto painted_properties=painted_json["extensions"]["VRM"]["materialProperties"][painted_material];
    auto main=texture_png(painted_json,painted_bytes,painted_properties["textureProperties"]["_MainTex"]);
    auto current=painting->composite(painted_scene.active().material);size_t sample=SIZE_MAX;
    auto color_view=current->source_maps->color;color_view.uv_transform={1,0,0,1,0,0};color_view.texcoord=0;
    color_view.image=std::shared_ptr<const Image>(current,&current->color);
    auto coverage_view=color_view;coverage_view.image=std::shared_ptr<const Image>(current,&current->color_coverage);
    for(uint32_t y=0;y<main.height && sample==SIZE_MAX;++y)for(uint32_t x=0;x<main.width;++x) {
      float u=(x+.5f)/main.width,v=(y+.5f)/main.height;
      if(sample_source_texture(coverage_view,u,v,false)[0]>.999f && sample_source_texture(color_view,u,v,true)[1]>.999f){sample=(size_t(y)*main.width+x)*4;break;}
    }
    check(sample!=SIZE_MAX && main.width==64 && main.height==64 &&
          main.rgba[sample]<4 && main.rgba[sample+1]>250 && main.rgba[sample+2]<4,
          "Actual retained paint exports independently decoded green VRM MainTex pixels");
    bool separate_fork=false;std::set<std::string> painted_names;
    for(const auto &m:painted_json.at("materials"))painted_names.insert(m.at("name"));
    for(size_t i=0;i<objects.size();++i)if(i!=shared_part && objects[i].appearance->source_material==objects[shared_part].appearance->source_material)
      separate_fork|=painted_json["meshes"][i]["primitives"][0]["material"].get<size_t>()!=painted_material;
    check(painted_report.painted_materials==1 && painted_report.materials>raw_report.materials && separate_fork &&
          painted_names.size()==painted_json.at("materials").size(),
          "A painted shared source material receives a distinct state fork without changing its other parts");
    check(painted_scene.encode()==painted_native && painted_objects[shared_part].appearance==objects[shared_part].appearance,
          "Painted avatar export leaves editable layers and complete original appearance archives exact");
    cgltf_data *painted_raw=nullptr;
    check(cgltf_parse_file(&options,painted_path.c_str(),&painted_raw)==cgltf_result_success,
          "Independent parser accepts the painted avatar and material state forks");
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> painted_data(painted_raw,cgltf_free);
    check(cgltf_load_buffers(&options,painted_raw,painted_path.c_str())==cgltf_result_success && cgltf_validate(painted_raw)==cgltf_result_success,
          "Independent glTF validation accepts all reconstructed paint images and fork references");
    auto shared_source=objects[shared_part].appearance->source_material;
    auto shared_name=original["materialProperties"][shared_source]["name"];
    auto expressions=source_metadata(painted_objects,[&](auto &json) {
      json["extensions"]["VRM"]["blendShapeMaster"]["blendShapeGroups"].push_back({
        {"name","Material fork fixture"},{"presetName","unknown"},{"binds",nlohmann::json::array()},
        {"materialValues",{{{"materialName",shared_name},{"propertyName","_RimColor"},{"targetValue",{.3,.4,.2,1}}}}}});
    });
    auto expression_path=directory/"painted-expression.vrm";export_vrm(expressions,expression_path,painted_options);
    auto [expression_json,expression_bytes]=container(expression_path);
    auto expression_binds=expression_json["extensions"]["VRM"]["blendShapeMaster"]["blendShapeGroups"].back()["materialValues"];
    std::set<std::string> expected_names,actual_names;
    for(size_t i=0;i<objects.size();++i)if(objects[i].appearance->source_material==shared_source) {
      auto id=expression_json["meshes"][i]["primitives"][0]["material"].template get<size_t>();
      expected_names.insert(expression_json["materials"][id]["name"]);
    }
    for(const auto &bind:expression_binds) {
      actual_names.insert(bind.at("materialName"));
      if(bind.at("propertyName")!="_RimColor" || bind.at("targetValue")!=nlohmann::json::array({.3,.4,.2,1}))
        throw std::runtime_error("Material expression expansion changed an unaffected property or target");
    }
    check(expected_names.size()>1 && expected_names==actual_names && expression_binds.size()==actual_names.size(),
          "Original material-value expressions expand exactly once to every distinct painted/raw state fork");
    auto impossible=source_metadata(painted_objects,[&](auto &json) {
      json["extensions"]["VRM"]["blendShapeMaster"]["blendShapeGroups"].push_back({
        {"name","Changed factor fixture"},{"presetName","unknown"},{"binds",nlohmann::json::array()},
        {"materialValues",{{{"materialName",shared_name},{"propertyName","_Color"},{"targetValue",{.3,.4,.2,1}}}}}});
    });
    check(rejects([&]{export_vrm(impossible,expression_path,painted_options);}) && read(expression_path)==expression_bytes,
          "Unrepresentable flattened-factor expression fails atomically instead of silently changing its effect");
    auto identity_names=source_metadata(objects,[&](auto &json) {
      auto &vrm=json["extensions"]["VRM"];vrm["materialProperties"][0]["name"]="";
      vrm["materialProperties"][1]["name"]="Source material 0";
      // Empty names are still exact original expression identities. Generated
      // output labels must not alias a different material's literal name.
      vrm["blendShapeMaster"]["blendShapeGroups"].push_back({
        {"name","Exact empty-name identity"},{"presetName","unknown"},{"binds",nlohmann::json::array()},
        {"materialValues",{{{"materialName",""},{"propertyName","_RimColor"},{"targetValue",{.1,.2,.3,1}}},
                           {{"materialName","Source material 0"},{"propertyName","_RimColor"},{"targetValue",{.4,.5,.6,1}}}}}});
    });
    auto name_path=directory/"identity-names.vrm";export_vrm(identity_names,name_path);
    auto [name_json,name_bytes]=container(name_path);
    auto name_binds=name_json["extensions"]["VRM"]["blendShapeMaster"]["blendShapeGroups"].back()["materialValues"];
    check(name_binds.size()==2 && name_binds[0]["materialName"]!=name_binds[1]["materialName"] &&
          name_binds[1]["materialName"]=="Source material 0" && name_binds[0]["targetValue"]==nlohmann::json::array({.1,.2,.3,1}),
          "Empty original material names and colliding generated labels preserve separate exact expression identities");
    auto explicit_objects=objects;auto resolved=material_binding(explicit_objects[shared_part]);
    auto orange=std::make_shared<Image>(Image{2,2,std::vector<uint8_t>(16)});
    for(size_t at=0;at<orange->rgba.size();at+=4){orange->rgba[at]=255;orange->rgba[at+1]=96;orange->rgba[at+2]=0;orange->rgba[at+3]=255;}
    resolved->roles[0].image=orange;
    resolved->material.textures[0].index=int(explicit_objects[shared_part].appearance->asset->textures.size()+1000);
    explicit_objects[shared_part].material_bindings=resolved;
    auto explicit_path=directory/"explicit-role-avatar.vrm";export_vrm(explicit_objects,explicit_path);
    auto [explicit_json,explicit_bytes]=container(explicit_path);
    auto explicit_id=explicit_json["meshes"][shared_part]["primitives"][0]["material"].get<size_t>();
    auto explicit_texture=explicit_json["extensions"]["VRM"]["materialProperties"][explicit_id]["textureProperties"]["_MainTex"].get<int>();
    auto explicit_pixels=texture_png(explicit_json,explicit_bytes,explicit_texture);
    check(explicit_pixels.width==2 && explicit_pixels.height==2 && explicit_pixels.rgba==orange->rgba,
          "Explicit resolved role images export independently of historical source texture indices");
    auto unsupported_uv=objects;auto derivative=material_binding(unsupported_uv[shared_part]);
    auto origin=std::make_shared<SourceUvOrigin>();origin->paint=reopened.object(objects[shared_part].id).painted;
    origin->appearance=objects[shared_part].appearance;origin->material=derivative->material;origin->roles=derivative->roles;
    derivative->uv_origin=origin;unsupported_uv[shared_part].material_bindings=derivative;
    check(rejects([&]{export_vrm(unsupported_uv,explicit_path);}) && read(explicit_path)==explicit_bytes,
          "Derived normal frames cannot silently replace original tangent and morph transport under deformation");
    size_t tinted_part = SIZE_MAX;
    for (size_t i = 0; i < raw_objects.size(); ++i)
      if (raw_objects[i].material.color[0] == 0) {
        tinted_part = i;
        break;
      }
    if (tinted_part == SIZE_MAX)
      throw std::runtime_error(
          "Actual fixture lacks explicit zero-factor channel");
    raw_objects[tinted_part].material.color[0] = .25f;
    auto tinted_path = directory / "tinted-avatar.vrm";
    export_vrm(raw_objects, tinted_path);
    auto tinted_bytes = read(tinted_path);
    auto tinted_json = nlohmann::json::parse(tinted_bytes.begin() + 20,
                                             tinted_bytes.begin() + 20 +
                                                 word(tinted_bytes, 12));
    auto tinted_material=tinted_json["meshes"][tinted_part]["primitives"][0]["material"].get<size_t>();
    auto tinted_properties =
        tinted_json["extensions"]["VRM"]["materialProperties"][tinted_material];
    float gamma = .25f <= .0031308f ? .25f * 12.92f
                                    : 1.055f * std::pow(.25f, 1 / 2.4f) - .055f;
    check(std::abs(
              tinted_properties["vectorProperties"]["_Color"][0].get<float>() -
              gamma) < 2e-6f,
          "Edited zero-source MToon channels use explicit absolute color "
          "without division or double gamma");
    auto posed = objects;
    for (auto &part : posed) {
      auto deformation = std::make_shared<DeformationState>(*part.deformation);
      deformation->pose.clear();
      for (auto &bone : deformation->skeleton->bones)
        deformation->pose.push_back(bone.rest);
      auto head = deformation->humanoid->joints.at("head");
      deformation->pose[head].rotation =
          (Quat{std::cos(.17f), std::sin(.17f), 0, 0} *
           deformation->pose[head].rotation)
              .unit();
      if (deformation->morphs && deformation->morphs->targets.size() > 37) {
        deformation->morph_weights.assign(deformation->morphs->targets.size(),
                                          0);
        deformation->morph_weights[37] = .6f;
      }
      part.deformation = deformation;
    }
    auto posed_path = directory / "posed.vrm";
    export_vrm(posed, posed_path);
    cgltf_data *pose_raw = nullptr;
    check(cgltf_parse_file(&options, posed_path.c_str(), &pose_raw) ==
              cgltf_result_success,
          "Independent parser accepts actual edited avatar pose");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> pose_data(pose_raw,
                                                                 cgltf_free);
    check(cgltf_load_buffers(&options, pose_raw, posed_path.c_str()) ==
                  cgltf_result_success &&
              cgltf_validate(pose_raw) == cgltf_result_success,
          "Independent validator accepts shared avatar pose");
    compare_skin(pose_raw, posed);
    auto source_head =
        original["firstPerson"]["firstPersonBone"].get<uint32_t>();
    auto head_found =
        std::find(appearance.source_joint_nodes.begin(),
                  appearance.source_joint_nodes.end(), source_head);
    if (head_found == appearance.source_joint_nodes.end())
      throw std::runtime_error("Source camera head lacks identity");
    auto pose_worlds =
        posed[0].deformation->skeleton->world(posed[0].deformation->pose);
    Vec3 camera_offset{original_offset["x"], original_offset["y"],
                       original_offset["z"]};
    auto camera_reference =
        appearance.source_basis_quaternion->inverse().rotate(transformed(
            pose_worlds[size_t(head_found -
                               appearance.source_joint_nodes.begin())]
                .data(),
            appearance.source_basis_quaternion->rotate(camera_offset *
                                                       fit_scale)));
    size_t camera_node = vrm["firstPerson"]["firstPersonBone"].get<size_t>();
    float camera_world[16];
    cgltf_node_transform_world(&pose_raw->nodes[camera_node], camera_world);
    auto exported_camera = vrm["firstPerson"]["firstPersonBoneOffset"];
    check(length(transformed(camera_world,
                             {exported_camera["x"], exported_camera["y"],
                              exported_camera["z"]}) -
                 camera_reference) < 2e-5f,
          "Independent posed first-person camera anchor follows restored "
          "source bone-local frame");

    check(true, "Independent source-derived head pose evaluates consistently "
                "across all19 meshes");
    auto corrupted = objects;
    auto corrupt_appearance =
        std::make_shared<CreatorAppearance>(*corrupted[0].appearance);
    corrupt_appearance->source_joint_nodes.clear();
    corrupted[0].appearance = corrupt_appearance;
    auto no_basis = objects;
    auto missing_basis =
        std::make_shared<CreatorAppearance>(*objects[0].appearance);
    missing_basis->source_basis_quaternion.reset();
    no_basis[0].appearance = missing_basis;
    check(rejects([&] { export_vrm(no_basis, path); }),
          "Legacy unknown source basis cannot export coordinate-dependent VRM "
          "metadata");
    auto before = read(path);
    auto ambiguous = objects;
    auto missing_coverage=std::make_shared<TextureMaps>(*reopened.object(objects[0].id).painted->composite(objects[0].material));
    missing_coverage->color_coverage={};ambiguous[0].textures=std::move(missing_coverage);
    check(rejects([&] { export_vrm(ambiguous, path); }) && read(path) == before,
          "Derived MToon without coverage rejects instead of silently losing "
          "shadow paint");

    auto nonlinear = objects;
    nonlinear[0].mesh.positions[0].x += .02f;
    auto edited = std::make_shared<DeformationState>(*nonlinear[0].deformation);
    edited->source = morph_topology_id(nonlinear[0].mesh);
    if (edited->morphs)
      edited->morphs->topology = edited->source;
    nonlinear[0].deformation = edited;
    check(rejects([&] { export_vrm(nonlinear, path); }) && read(path) == before,
          "Nonlinear rest edits reject until local physics metadata is "
          "explicitly refitted");

    check(rejects([&] { export_vrm(corrupted, path); }) && read(path) == before,
          "Missing legacy joint map cannot replace an existing valid avatar "
          "file");
    VrmExportOptions cancelled;
    cancelled.cancelled = [] { return true; };
    check(rejects([&] { export_vrm(objects, path, cancelled); }) &&
              read(path) == before,
          "Cancellation preserves existing avatar output exactly");
    std::cout << "AVATAR " << gender << " bytes=" << bytes.size()
              << " joints=" << raw->skins[0].joints_count << " output=" << path
              << '\n';
  }
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
