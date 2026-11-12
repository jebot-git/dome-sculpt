// SPDX-License-Identifier: GPL-3.0-or-later
#define main established_generic_rig_fixture
#include "rig.cpp"
#undef main
#include "dome/skin-authoring.hpp"
namespace {
Mesh tapered_pair() {
  Mesh mesh;
  constexpr uint32_t rings=21,sides=16;
  for(uint32_t component=0;component<2;++component){
    const uint32_t start=mesh.positions.size();
    const float center=component?.12f:-.12f;
    for(uint32_t row=0;row<rings;++row){
      const float y=float(row)/(rings-1),radius=.008f+.052f*y;
      for(uint32_t i=0;i<sides;++i){
        const float angle=float(2*std::acos(-1.)*i/sides);
        mesh.positions.push_back({center+radius*std::cos(angle),y,radius*std::sin(angle)});
        mesh.normals.push_back(normalized(Vec3{std::cos(angle),-.052f,std::sin(angle)}));
      }
    }
    for(uint32_t row=0;row+1<rings;++row)for(uint32_t i=0;i<sides;++i){
      const auto a=start+row*sides+i,b=start+row*sides+(i+1)%sides,c=a+sides,d=b+sides;
      mesh.indices.insert(mesh.indices.end(),{a,b,c,b,d,c});
    }
    for(uint32_t i=1;i+1<sides;++i){
      mesh.indices.insert(mesh.indices.end(),{start,start+i+1,start+i});
      const auto top=start+(rings-1)*sides;
      mesh.indices.insert(mesh.indices.end(),{top,top+i,top+i+1});
    }
  }
  mesh.validate();unwrap(mesh,64,0);return mesh;
}
Mesh left_only(const Mesh &source){
 Mesh out;std::map<uint32_t,uint32_t>remap;
 for(size_t i=0;i<source.indices.size();i+=3){
  if(source.positions[source.indices[i]].x>=0)continue;
  for(size_t k=0;k<3;++k){auto v=source.indices[i+k];auto[it,added]=remap.emplace(v,uint32_t(out.positions.size()));if(added){out.positions.push_back(source.positions[v]);out.normals.push_back(source.normals[v]);out.uv.push_back(source.uv[v]);}out.indices.push_back(it->second);}
 }out.validate();return out;
}
Skeleton declared_pair(){return Skeleton{{{"controlA",-1,{{-.12f,0,0}},1},{"controlB",-1,{{.12f,0,0}},1}}};}
HumanoidMapping roles(){return HumanoidMapping{{{"leftUpperLeg",0},{"rightUpperLeg",1}}};}
Scene scene(const Mesh &mesh,const Skeleton &rig,const HumanoidMapping &mapping){Scene s;s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(mesh,64);DeformationState state(mesh);state.skeleton=rig;state.humanoid=mapping;s.set_deformation(1,state);return s;}
SkinAuthoringOptions options(){SkinAuthoringOptions o;o.reference=1;return o;}
void only_authorized(const Scene &after,const Scene &before){auto reverted=after;reverted.object(1).deformation=before.object(1).deformation;reverted.object(1).rig_source=before.object(1).rig_source;require(reverted.encode()==before.encode(),"Mapped automatic bind changes only authorized skin/source fields");}
}
int main()try{
 auto mesh=tapered_pair();auto rig=declared_pair();auto s=scene(mesh,rig,roles());const auto before=s;
 require(author_skin(s,1,options()),"Actual public Automatic binds a declared native mirrored-limb rig");only_authorized(s,before);
 const auto &weights=*s.active().deformation->skin;bool own=true;
 for(size_t v=0;v<weights.vertices.size();++v){const uint32_t expected=mesh.positions[v].x<0?0:1;own&=weights.vertices[v]==VertexWeights{{expected,1}};}
 require(own,"Whole tapered components with strict unanimous nearest family receive no opposite-leg seeds");
 std::vector<JointTransform>pose;for(const auto&b:rig.bones)pose.push_back(b.rest);const double angle=std::acos(-1.)/3;pose[0].rotation={float(std::cos(angle/2)),0,0,float(std::sin(angle/2))};auto deformed=deform_mesh(mesh,weights,rig,pose);double maximum=0;
 for(size_t v=0;v<mesh.positions.size();++v){auto p=mesh.positions[v];Vec3 expected=p;if(p.x<0){const auto x=p.x+.12f;expected={float(-.12+std::cos(angle)*x-std::sin(angle)*p.y),float(std::sin(angle)*x+std::cos(angle)*p.y),p.z};}maximum=std::max(maximum,double(length(deformed.positions[v]-expected)));}
 require(maximum<2e-6,"Actual native deform_mesh keeps the declared second component fixed and first component rigid");
 // Keep the existing intrinsic hairpin contract even with explicit role IDs.
 auto hp=hairpin();unwrap(hp,64,0);Skeleton arms{{{"A",-1,{{.003f,0,0}},1},{"B",-1,{{.028f,0,0}},.1f}}};auto generic_options=BindOptions{};generic_options.maximum_seed_distance=.05f;generic_options.seed_margin=.0001f;auto expected=automatic_bind(hp,arms,generic_options);auto mixed=scene(hp,arms,roles());auto op=options();op.seed_distance=.05f;op.seed_margin=.0001f;author_skin(mixed,1,op);
 require(mixed.active().deformation->skin->vertices==expected.vertices&&weight(expected.vertices[10],1)>weight(expected.vertices[10],0),"Mixed-family connected hairpin keeps original intrinsic graph policy byte-for-byte");
 auto tied_rig=rig;tied_rig.bones[1].rest=tied_rig.bones[0].rest;auto tied=scene(mesh,tied_rig,roles());auto generic=automatic_bind(mesh,tied_rig);author_skin(tied,1,options());require(tied.active().deformation->skin->vertices==generic.vertices,"Exact cross-family distance ties preserve all original intrinsic sources");
 auto missing=roles();missing.joints.erase("rightUpperLeg");auto incomplete=scene(mesh,rig,missing);generic=automatic_bind(mesh,rig);author_skin(incomplete,1,options());require(incomplete.active().deformation->skin->vertices==generic.vertices,"An absent mirrored role cannot invent a family assignment");
 auto locked_mesh=left_only(mesh);auto locked=scene(locked_mesh,rig,roles());author_skin(locked,1,options());auto authored=*locked.active().deformation;for(auto &row:authored.skin->vertices)row={{0,.8f},{1,.2f}};locked.set_deformation(1,authored);auto lock=options();lock.locks={0,1};author_skin(locked,1,lock);bool exact=true;for(const auto&row:locked.active().deformation->skin->vertices)exact&=weight(row,1)==.2f;require(exact,"Positive user locks override prospective family support without changing their mass");
 auto immutable=locked.encode();lock.locks={1,1};lock.maximum_work=1;require(!author_skin(locked,1,lock)&&locked.encode()==immutable,"Fully locked mapped bind remains an exact no-op before tiny-budget analysis");
 auto bounded=options();bounded.maximum_work=1;rejects([&]{author_skin(locked,1,bounded);},"New exact source-family analysis obeys the original work ceiling");require(locked.encode()==immutable,"Mapped source-family budget refusal preserves the complete accepted scene");
 rejects([&]{author_skin(locked,1,options(),[]{return true;});},"Mapped automatic binding retains native cancellation");require(locked.encode()==immutable,"Mapped cancellation preserves complete source, rig, locks, paint and history authority");
 require(Scene::decode(s.encode()).encode()==s.encode(),"Accepted mapped source-family weights restart exactly");std::cout<<"PASS native mapped component support controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
