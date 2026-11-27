// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_RIG_AFTER_AUTHORING_HELPERS_ONLY
#include "rig-after-authoring.cpp"
#include "dome/shapekey-authoring.hpp"
#include "dome/native-scene-append.hpp"
#include "native-paint-frame-source.hpp"
#include "normal-key-derivation.hpp"
#include "native-surface-heat.hpp"
namespace {
void shape(Document&doc,ShapeKeyEdit edit,const std::filesystem::path&path){doc.author_shape(edit);doc.save_async(path).get();ready(doc);}

void bind_authored_frames(Input input,const std::filesystem::path &out,float seed_distance,bool retained_color=false) {
 std::filesystem::create_directories(out);
 input.scene.save(out/"before.dsculpt");
 const auto source=Scene::load(out/"before.dsculpt");
 const auto &original=source.active();
 const auto &bindings=original.material_bindings;
 demand(bindings&&bindings->key_derivation&&bindings->normal_frames&&
        bindings->key_derivation->position_domain(),
        "The cold input owns an actual separate POSITION and artist-key domain");
 if(retained_color){
  demand(original.polygon&&original.polygon->colors&&bindings->color_preparation&&
         bindings->color_preparation->paint_sampling&&bindings->normal_preparation&&
         bindings->normal_preparation->sampling_domain&&
         bindings->normal_preparation->sampling_domain->has_sealed_metadata(),
         "Cold input composes real writable, corner colour and sealed prepared normal domains");
 }
 const auto exact=source.encode();
 auto options=input.options;
 options.mode=SkinAuthoringMode::SurfaceHeat;
 options.seed_distance=seed_distance;
 auto expected=source;
 demand(author_skin(expected,input.target,options),"Surface Heat solves the actual authored rest geometry");
 const auto &bound=expected.active();
 same_paint(bound,original);
 demand(bound.deformation->skin&&bound.deformation->morphs->encode()==original.deformation->morphs->encode()&&
        bound.deformation->morph_weights==original.deformation->morph_weights&&
        bound.material_bindings->key_derivation==bindings->key_derivation&&
        bound.material_bindings->normal_frames==bindings->normal_frames&&bound.polygon==original.polygon,
        "Solved influences keep the exact active morphs, logical corners and artist frame owners");
 auto unrelated=expected;
 unrelated.active().deformation=original.deformation;
 unrelated.active().rig_source=original.rig_source;
 demand(unrelated.encode()==exact,"Only skin and its original rig capsule change on publication");

 detail::NativeSurfaceHeatOptions bounds;
 bounds.seed_distance=options.seed_distance;
 const auto retained=detail::native_weight_retention(source,bounds.maximum_total-bounds.maximum_workspace);
 const std::array selected{input.target};
 bounds.maximum_total=bounds.maximum_workspace+retained.bytes;
 auto request=detail::capture_native_surface_heat(source,selected,input.reference,bounds);
 demand(request.matches(source),"The exact retained byte budget admits the cold owning request");
 --bounds.maximum_total;
 bool short_input=false;
 try{(void)detail::capture_native_surface_heat(source,selected,input.reference,bounds);}
 catch(const std::length_error&){short_input=true;}
 demand(short_input,"One byte below the retained budget is refused before staging");
 auto candidate=source;
 auto prepared=detail::prepare_native_surface_heat(request);
 demand(prepared.report().retained_input==retained.bytes&&prepared.report().changed==1,
        "The worker reports the measured retention and one solved mesh");
 bool cancelled=false;
 demand(denied([&]{detail::adopt_native_surface_heat(candidate,request,std::move(prepared),
          [&]{cancelled=true;return true;});})&&cancelled&&candidate.encode()==exact,
        "Cancelled worker publication preserves every original owner and authored value");

 Document doc;
 doc.open_async(out/"before.dsculpt").get();
 ready(doc);
 doc.bind_skin(options);
 doc.save_async(out/"bound.dsculpt").get();
 ready(doc);
 const auto solved=expected.encode();
 demand(doc.copy_scene().encode()==solved&&Scene::load(out/"bound.dsculpt").encode()==solved,
        "The document worker and immediately queued Save match the detached binding");
 demand(doc.undo()&&doc.copy_scene().encode()==exact&&!doc.frame().state->can_undo&&
        doc.redo()&&doc.copy_scene().encode()==solved,"Bind Skin is one exact native Undo and Redo");
}

void heat_frames(const std::filesystem::path &out,const std::filesystem::path &native,
                 const std::filesystem::path &prepared,const std::filesystem::path &retained) {
 auto input=with_reference(native_case(false));
 ShapeKeyEdit key;
 key.name="Independent source seam";
 author_shapekey(input.scene,input.target,key);
 key.action=ShapeKeyAction::Replace;
 key.target=0;
 key.positions=input.scene.active().painted->mesh().positions;
 key.positions.front().z+=.014f;
 author_shapekey(input.scene,input.target,key);
 auto keyed=*input.scene.active().deformation;
 keyed.morph_weights={.6f};
 input.scene.set_deformation(input.target,std::move(keyed));
 demand(!input.scene.active().deformation->skin,"Independent writable frames are authored before the first bind");
 bind_authored_frames(std::move(input),out/"native-first",.3f);
 bind_authored_frames(with_reference(Scene::load(native)),out/"native-rebind",.3f);
 bind_authored_frames(with_reference(Scene::load(prepared)),out/"prepared-mirror",1);
 bind_authored_frames(with_reference(Scene::load(retained)),out/"retained-mirror",1,true);
 std::cout<<"PASS authored Surface Heat checks="<<rig_checks<<std::endl;
}
}
int main(int argc,char**argv)try{if(argc!=2&&(argc!=6||std::string_view(argv[1])!="--surface-heat"))return 2;
 const std::filesystem::path out=argv[argc==2?1:2];std::filesystem::create_directories(out);setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 if(argc==6){heat_frames(out,argv[3],argv[4],argv[5]);return 0;}
 auto input=with_reference(native_case(false));demand(author_skin(input.scene,input.target,input.options),"Actual ordinary binding precedes independent native32 key capture");const auto before=input.scene.encode();const auto source=input.scene.active();const auto original=source.material_bindings->uv_origin;Document doc;doc.replace(input.scene);ready(doc);
 ShapeKeyEdit create;create.action=ShapeKeyAction::Create;create.name="Genuine current native key";shape(doc,create,out/"created.dsculpt");auto made=doc.copy_scene();const auto b=made.active().material_bindings;const auto frame=b->native_paint_frames;
 demand(frame&&frame->valid_owner(frame)&&b->native_paint_sampling==source.material_bindings->native_paint_sampling&&frame->before_material()->uv_origin==original&&b->uv_origin==frame->origin()&&b->uv_origin!=original,"Independent current normal origin and historical original paint origin are both genuine owning records");
 demand(made.active().rig_source==source.rig_source&&made.active().shape_source&&made.active().painted->mesh_owner()==source.painted->mesh_owner()&&detail::NativeWeightPaintAccess::same_current(*made.active().painted,*source.painted),"Native key keeps current writable grids/REST and first rig/shape source");
 const auto encoded=made.encode();demand(encoded.at("version")==36&&encoded.at("source_materials").at("version")==18,"Actual owning same-object native paint/frame uses conditional Scene36/material18");
 demand(Scene::load(out/"created.dsculpt").encode()==encoded,"Actual accepted key Save/reopen preserves every native/normal source alias");
 ShapeKeyEdit replace;replace.action=ShapeKeyAction::Replace;replace.target=0;replace.positions=made.active().painted->mesh().positions;replace.positions[0].z+=.01f;shape(doc,replace,out/"replaced.dsculpt");auto changed=doc.copy_scene();demand(changed.active().deformation->morphs->targets[0].delta[0].z==.01f&&changed.active().deformation->morphs->targets[0].delta[3].z==0,"Distinct current seam vertices keep independent POSITION endpoints, without original paint-vertex averaging");
 demand(changed.active().material_bindings->key_derivation&&changed.active().material_bindings->native_paint_frames->keys()==changed.active().material_bindings->key_derivation&&changed.active().material_bindings->native_paint_frames->frames()==changed.active().material_bindings->normal_frames,"Actual derived key owns its independently replayed current frame seal");
 demand(doc.undo()&&doc.copy_scene().encode()==encoded&&doc.undo()&&doc.copy_scene().encode()==before&&!doc.frame().state->can_undo&&doc.redo()&&doc.redo()&&doc.copy_scene().encode()==changed.encode(),"Create and genuine Replace each own one exact Undo/Redo endpoint");
 auto untouched=changed;ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(untouched,input.target,restore);demand(untouched.encode()==before,"Full Shape Restore returns exact pre-key native32 skin and original paint history");
 auto bad=encoded;bad["version"]=35;demand(denied([&]{Scene::decode(bad);}),"Scene35 cannot silently consume native frame36");bad=encoded;bad["source_materials"]["version"]=17;demand(denied([&]{Scene::decode(bad);}),"Material17 cannot silently ignore native frame source18");bad=encoded;bad["source_materials"]["native_frame_sources"].push_back(bad["source_materials"]["native_frame_sources"][0]);demand(denied([&]{Scene::decode(bad);}),"Orphan independent native normal capture refuses");
 auto forged=made;auto mutable_binding=std::make_shared<SourceMaterialBindings>(*forged.active().material_bindings);mutable_binding->uv_origin=original;forged.active().material_bindings=mutable_binding;demand(denied([&]{forged.validate();}),"Native normal seal rejects copied binding that substitutes the older paint origin");
 // These actual decoded FrameSet/NormalFrames are allocated mutable by the
 // native private replay factories, then exposed through const views. The
 // probes are legal same-address scalar/owner changes, never writes into a
 // truly const capture origin or mutation of immutable geometry arrays.
 {auto probe=Scene::load(out/"replaced.dsculpt");const auto baseline=probe.encode();const auto bound=probe.active().material_bindings;const auto leaf=bound->native_paint_frames;auto frames=std::const_pointer_cast<NormalFrameSet>(bound->normal_frames);auto role=std::const_pointer_cast<NormalFrames>(frames->pbr);const auto transport=role->transport;role->transport=transport==NormalFrameTransport::FixedAttributes?NormalFrameTransport::CanonicalMikk:NormalFrameTransport::FixedAttributes;demand(denied([&]{leaf->validate_binding(*bound,false);}),"Cheap private certificate rejects valid-enum scalar change at the same real mutable frame address");role->transport=transport;
  const auto toon=frames->mtoon;frames->mtoon=std::shared_ptr<const NormalFrames>(toon.get(),[](const NormalFrames*){});demand(denied([&]{leaf->validate_binding(*bound,false);}),"Cheap private certificate rejects same-address foreign normal role control block");frames->mtoon=toon;
  auto typed=*bound;typed.has_mtoon=true;demand(denied([&]{leaf->validate_binding(typed,false);}),"Normal certificate cannot authorize an unrelated current material role");
  auto style=*bound;style.physical_emission=std::array<float,3>{1,.5f,.1f};leaf->validate_binding(style,false);demand(probe.encode()==baseline,"Independent normal seal permits unrelated authored style and recovers exact probe state");
 }
 {auto canonical=canonical_case(true,true,false);const auto canonical_before=canonical.encode(),native_before=changed.encode();auto appended=append_native_scene(canonical,changed);const auto mixed=appended.scene.encode();demand(mixed.at("version")==36&&mixed.at("source_materials").at("version")==18&&mixed.at("source_materials").at("layered_normals")==true&&mixed.at("source_materials").at("retained_canonical_uv")==true&&mixed.contains("baked_normal_fields"),"Real native package append composes native frame36 and actual converted UV/normal-field actors using genuine linked capabilities");demand(Scene::decode(mixed).encode()==mixed&&canonical.encode()==canonical_before&&changed.encode()==native_before,"Complete mixed36 native restart preserves both independent owning graphs and input scenes");auto false_context=mixed;false_context["source_materials"]["layered_normals"]=false;demand(denied([&]{Scene::decode(false_context);}),"Numeric Scene36 cannot grant a missing actual canonical normal-field context");}
 std::cout<<"PASS native_paint_key_frame_checks="<<rig_checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
