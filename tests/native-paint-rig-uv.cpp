// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_RIG_AFTER_AUTHORING_HELPERS_ONLY
#include "rig-after-authoring.cpp"
#include "dome/shapekey-authoring.hpp"
#include "native-paint-model.hpp"
#include <numeric>
namespace {
void model_equal(const SculptObject &before,const SculptObject &after) {
 const auto &a=*before.deformation,&b=*after.deformation;
 demand(a.skeleton->encode()==b.skeleton->encode(),"Native UV keeps actual skeleton/rest/IBM/space authority exact");
 auto old=a.preview(before.painted->mesh()),now=b.preview(after.painted->mesh());
 demand(old.indices.size()==now.indices.size(),"Native UV keeps complete actual posed triangle correspondence");
 for(size_t c=0;c<old.indices.size();++c){const auto x=old.positions[old.indices[c]],y=now.positions[now.indices[c]];demand(x.x==y.x&&x.y==y.y&&x.z==y.z,"Independent old/new posed corner POSITION is exact");const auto n=old.normals[old.indices[c]],m=now.normals[now.indices[c]];demand(n.x==m.x&&n.y==m.y&&n.z==m.z,"Independent old/new authored posed corner NORMAL is exact");}
 demand(before.rig_source==after.rig_source&&before.shape_source==after.shape_source&&before.bake_recipe==after.bake_recipe,"Native UV preserves genuine first rig/shape/recipe capsules");
}
void uv_apply(Document &doc){doc.begin_uv();demand(doc.wait_uv()&&doc.frame().state->uv->paint_sampling_available,"Actual bound native UV offers authenticated Keep paint detail");if(!doc.frame().state->uv->retain_paint_sampling)doc.edit_uv(UvAction::RetainPaint);doc.edit_uv(UvAction::Pick,.5f,.5f);doc.edit_uv(UvAction::Scale,.8f);doc.edit_uv(UvAction::Rotate,.3f);demand(doc.wait_uv()&&doc.frame().state->uv->valid,"Actual bound native UV draft remains valid");doc.end_uv(true);}
}
#ifndef DOME_NATIVE_PAINT_RIG_UV_HELPERS_ONLY
int main(int argc,char**argv)try{if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 auto in=with_reference(native_case(false));demand(author_skin(in.scene,in.target,in.options),"Actual ordinary transfer binds independent native paint");
 auto pose=std::make_shared<DeformationState>(*in.scene.active().deformation);for(const auto &bone:pose->skeleton->bones)pose->pose.push_back(bone.rest);
 pose->pose[1].rotation=arcball({0,1,0},{.3f,.9539392f,0});
 const RigMatrix affine{1,.1f,0,0,.2f,1.1f,0,0,0,0,.9f,0,.01f,.02f,0,1};
 pose->skeleton->space_matrix=affine;pose->skeleton->inverse_bind_override=pose->skeleton->inverse_bind();pose->skeleton->bones[1].rest_matrix=affine;pose->pose[1].full_local_matrix=affine;
 in.scene.active().deformation=pose;in.scene.validate();const auto original=in.scene.encode();const auto source=in.scene.active();
 Mesh target;const auto &mesh=source.painted->mesh();std::vector<uint32_t> map(mesh.positions.size());std::iota(map.rbegin(),map.rend(),0u);std::vector<uint32_t> inverse(map.size());for(size_t v=0;v<map.size();++v){inverse[map[v]]=v;target.positions.push_back(mesh.positions[map[v]]);target.normals.push_back(mesh.normals[map[v]]);const auto uv=mesh.uv[map[v]];target.uv.push_back({.5f-.8f*(uv[1]-.5f),.5f+.8f*(uv[0]-.5f)});}for(auto v:mesh.indices)target.indices.push_back(inverse[v]);
 auto result=detail::retain_native_paint_model(source,target,map);auto current=in.scene;current.active().painted=result.sampling.paint;current.active().material_bindings=detail::bind_native_paint(result.sampling.owner,source.material_bindings);current.active().deformation=result.deformation;current.validate();model_equal(source,current.active());demand(current.active().deformation!=source.deformation,"Nonidentity vertex correspondence owns genuinely remapped native skin rows");for(size_t v=0;v<map.size();++v)demand(current.active().deformation->skin->vertices[v]==source.deformation->skin->vertices[map[v]],"Each remapped native row equals the actual declared source row");
 auto bad=map;std::swap(bad[0],bad[1]);demand(denied([&]{detail::retain_native_paint_model(source,target,bad);})&&in.scene.encode()==original,"Forged current/source vertex mapping refuses without source mutation");
 PaintSamplingLimits tiny;tiny.max_working_bytes=65536;tiny.max_basis_bytes=65536;demand(denied([&]{tiny.max_work=1;detail::retain_native_paint_model(source,target,map,tiny);})&&in.scene.encode()==original,"Actual combined remap/paint budget refusal is atomic");
 {auto unsupported=source;auto morph=std::make_shared<DeformationState>(*source.deformation);morph->morphs=MorphSet(mesh);morph->morphs->set_target(mesh,"Not yet paired native key",mesh.positions);unsupported.deformation=morph;demand(!detail::native_paint_retention_supported(unsupported)&&denied([&]{detail::retain_native_paint_model(unsupported,target,map);}),"Unpaired native morph/frame authority stays explicitly refused");}
 Document doc;doc.replace(in.scene);ready(doc);uv_apply(doc);doc.save_async(out/"bound-uv.dsculpt").get();ready(doc);auto once=doc.copy_scene();model_equal(source,once.active());demand(Scene::load(out/"bound-uv.dsculpt").encode()==once.encode(),"Actual bound native UV and accepted Save reopen exact complete history");demand(doc.undo()&&doc.copy_scene().encode()==original&&!doc.frame().state->can_undo&&doc.redo()&&doc.copy_scene().encode()==once.encode(),"Actual bound UV is exactly one Undo/Redo");
 uv_apply(doc);doc.save_async(out/"twice.dsculpt").get();ready(doc);auto twice=doc.copy_scene();model_equal(source,twice.active());demand(twice.active().material_bindings->native_paint_sampling->history_depth()==3&&twice.active().rig_source==source.rig_source,"Repeated actual UV keeps ordered paint and first rig provenance");
 doc.begin_uv();demand(doc.wait_uv(),"Bound native Cancel workspace ready");doc.edit_uv(UvAction::Scale,.9f);doc.end_uv(false);demand(doc.copy_scene().encode()==twice.encode(),"Bound UV draft Cancel preserves complete skin/pose/paint/history");
 doc.edit_scene("Restore genuine pre-bind source",[id=in.target](Scene&s,const auto&cancel){restore_rig_source(s,id,false,cancel);});doc.save_async(out/"restored.dsculpt").get();ready(doc);auto expected=in.scene;restore_rig_source(expected,in.target,false);demand(doc.copy_scene().encode()==expected.encode()&&Scene::load(out/"restored.dsculpt").encode()==expected.encode(),"Full rig Restore after repeated UV keeps authentic complete first source");
 std::cout<<"PASS native_paint_rig_uv_checks="<<rig_checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}

#endif
