// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_shapekey_kernel_test
#include "shapekey-authoring.cpp"
#undef main
#include "dome/shapekey-sculpt.hpp"
namespace {
const RigMatrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
Vec3 transformed(const RigMatrix&m,Vec3 p){return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}
ShapeSculptDraft draft(const Scene&s,const RigMatrix &world=identity,ShapeSculptOptions options={}){return {s.active().painted,s.active().deformation,0,world,options};}
Brush brush(){Brush b;b.from=b.to={-1,-1,0};b.normal={0,0,1};b.radius=.5f;return b;}
}
int main()try{
 auto s=fixture();author_shapekey(s,s.active().id,{});auto before=s.encode();auto b=brush();auto d=draft(s);
 check(d.stamp(b)&&std::abs(d.positions()[0].z-.075f)<1e-6f&&d.positions()[1].z==0,"Clay produces a bounded target displacement inside the world-space brush only");
 auto endpoint=std::vector<Vec3>(d.positions().begin(),d.positions().end());check(!d.stamp(b)&&same_vectors(endpoint,d.positions()),"Repeated stationary controller samples do not inflate a target with frame rate");
 auto carving=draft(s);b.tool=Tool::Carve;carving.stamp(b);check(std::abs(carving.positions()[0].z+.075f)<1e-6f,"Carve moves the selected key opposite the visible surface normal");
 b=brush();b.pressure=.25f;auto pressure=draft(s);pressure.stamp(b);check(std::abs(pressure.positions()[0].z-.01875f)<1e-6f,"Pressure scales displacement without changing brush radius");
 auto posed=pressure.preview();check(std::abs(posed.positions[0].z-.01875f)<1e-6f&&posed.indices==s.active().painted->mesh().indices&&posed.uv==s.active().painted->mesh().uv,"Draft preview preserves exact topology and UV identity");
 check(s.encode()==before,"Detached target strokes leave live paint, keys, weights, rig and original source exact");
 auto cancelled=draft(s);b=brush();check(refuses([&]{cancelled.stamp(b,[]{return true;});})&&!cancelled.changed()&&same_vectors(cancelled.positions(),s.active().painted->mesh().positions),"Cancelled gesture retains exact source-domain positions");
 auto invalid=b;invalid.strength=2;check(refuses([&]{cancelled.stamp(invalid);})&&!cancelled.changed(),"Invalid brush settings do not partially mutate the draft");
 auto limit=draft(s,identity,{4,19,true});check(refuses([&]{limit.stamp(b);})&&!limit.changed(),"Cumulative work budget failure preserves the complete original draft");
 auto edited=s;ShapeKeyEdit replacement;replacement.action=ShapeKeyAction::Replace;replacement.positions=edited.active().painted->mesh().positions;replacement.positions[0].z=.2f;author_shapekey(edited,edited.active().id,replacement);
 auto flattened=draft(edited);b=brush();b.tool=Tool::Flatten;b.radius=1;flattened.stamp(b);check(std::abs(flattened.positions()[0].z)<.03f,"Flatten moves a raised key toward the brush plane with smooth falloff");
 auto smooth=draft(edited);b.tool=Tool::Smooth;b.from=b.to={-1,-1,.2f};b.radius=.5f;smooth.stamp(b);check(std::abs(smooth.positions()[0].z)<1e-6f,"Smooth uses fixed gesture-source neighbors instead of frame-dependent feedback");
 auto deform=*s.active().deformation;Skeleton skeleton;Bone first;first.name="First";Bone second;second.name="Second";skeleton.bones={first,second};deform.skeleton=skeleton;SkinWeights weights;weights.vertices.assign(4,{{0,.25f},{1,.75f}});deform.skin=weights;deform.pose.resize(2);deform.pose[0].scale={2,1,.5f};deform.pose[1].scale={1,3,2};deform.pose[0].rotation={std::cos(.2f),0,0,std::sin(.2f)};deform.pose[1].rotation={std::cos(.1f),0,std::sin(.1f),0};s.set_deformation(s.active().id,deform);
 RigMatrix world{1.7f,.1f,0,0,.35f,1.2f,.15f,0,.1f,-.2f,.8f,0,2,-.5f,1,1};
 auto base=deform.preview(s.active().painted->mesh());auto center=transformed(world,base.positions[0]);
 auto tangent0=transformed(world,base.positions[1])-center,tangent1=transformed(world,base.positions[3])-center;auto normal=normalized(cross(tangent0,tangent1));
 b=brush();b.from=b.to=center;b.normal=normal;b.radius=.2f;auto mixed=draft(s,world);mixed.stamp(b);auto actual=transformed(world,mixed.preview().positions[0])-center;
 check(length(actual-normal*.03f)<2e-6f,"Mixed anisotropic skin plus affine world transform yields the requested world-space displacement");
 check(s.active().deformation->encode(s.active().painted->mesh())==deform.encode(s.active().painted->mesh()),"Posed sculpting does not bake pose, inverse binds or preview weights into the key source");
 auto singular=identity;singular[0]=0;check(refuses([&]{auto unused=draft(s,singular);}),"A singular world transform is rejected instead of inventing an inverse");
 ShapeKeyEdit apply;apply.action=ShapeKeyAction::Replace;apply.positions.assign(mixed.positions().begin(),mixed.positions().end());author_shapekey(s,s.active().id,apply);
 check(s.active().shape_source&&s.active().material_bindings->normal_frames->current_targets.size()==1&&s.active().deformation->pose.size()==2,"Actual source-retaining authoring accepts sculpt positions with stable frame identity and pose");
 std::cout<<"PASS "<<checks<<" shapekey sculpt checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
