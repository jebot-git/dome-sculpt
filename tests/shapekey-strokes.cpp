// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_shapekey_kernel_test
#include "shapekey-authoring.cpp"
#undef main
#include "dome/document.hpp"
namespace {
void settled(Document &d){check(d.wait_native_stroke()&&d.wait_asset()&&d.wait_mesh(),"Ordered target gesture and immutable surface finish");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
NativeStrokeSample ray(){NativeStrokeSample s;s.origin={-.95f,-.95f,2};s.direction={0,0,-1};s.brush.radius=.5f;s.brush.tool=Tool::Clay;return s;}
struct Hand{
 Document &doc;SpatialFrame f;
 explicit Hand(Document &d):doc(d){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};auto &h=f.hands[1].tracked;h.valid=h.aimValid=true;send();h.grip.position=h.aim.position=ob::transform(doc.frame().state->canvas,{-.95f,-.95f,2});h.grip.orientation=h.aim.orientation=doc.frame().state->canvas.orientation;send();}
 void send(float x=0,float y=0,bool click=false,float pressure=0){++f.sequence;f.time+=.05;auto &h=f.hands[1];h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=pressure;doc.submit(f);if(!doc.wait_input())throw std::runtime_error("Shape stroke input timeout");}
 void sector(int i){float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
 void shapes(){send();send(0,0,true);send();sector(8);sector(9);sector(7);}
};
}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;auto s=fixture();author_shapekey(s,s.active().id,{});auto original=s.encode();auto paint=s.active().painted->encode();
 Document doc;doc.replace(s);settled(doc);doc.configure_shape_sculpt(true);settled(doc);
 check(doc.frame().state->shape_sculpt&&doc.copy_scene().encode()==original&&!doc.frame().state->dirty,"Entering selected-key sculpt changes only the viewport and input mode");
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(ray()),"Desktop target editing uses the actual ordered gesture worker");doc.end_native_stroke();settled(doc);auto authored=doc.copy_scene().encode();
 check(authored!=original&&doc.copy_scene().active().deformation->morphs->targets[0].delta[0].z>0&&doc.copy_scene().active().painted->encode()==paint,"Accepted desktop stroke modifies selected target geometry and retains exact original paint");
 check(doc.copy_scene().active().deformation->morph_weights.empty()&&doc.frame().state->shape_sculpt,"One-hot editing preview never replaces saved morph weights");
 check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==authored,"One global Undo/Redo restores exact complete shape endpoints");settled(doc);
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(ray()),"A subsequent target stroke can begin");doc.end_native_stroke(false);settled(doc);check(doc.copy_scene().encode()==authored,"Cancel removes every draft displacement and frame change");
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(ray()),"Accepted target can be captured by an immediately ordered save");doc.end_native_stroke();auto path=std::filesystem::path(argv[1]);path+=".dsculpt";auto saved=doc.save_async(path);saved.get();settled(doc);
 check(Scene::load(path).encode()==doc.copy_scene().encode(),"Immediate Save includes the exact accepted shape endpoint rather than the earlier scene");
 doc.configure_shape_sculpt(false);settled(doc);check(!doc.frame().state->shape_sculpt&&doc.copy_scene().active().deformation->morph_weights.empty(),"Leaving sculpt restores the saved mixed preview without mutating source weights");
 Document xr;xr.replace(s);settled(xr);Hand hand(xr);hand.shapes();hand.sector(11);settled(xr);
 check(xr.frame().state->shape_sculpt&&!xr.frame().state->menu&&xr.frame().state->mesh_active,"Joystick Shape/Edit opens target sculpt while keeping the native mesh active");
 hand.send(0,0,false,.8f);check(xr.frame().state->native_pending,"Actual controller pressure admits a target gesture");hand.send();settled(xr);auto xr_authored=xr.copy_scene().encode();
 check(xr_authored!=original&&xr.copy_scene().active().deformation->morphs->targets[0].delta[0].z>0&&xr.copy_scene().active().painted->encode()==paint,"Controller ray/pressure edits the same target geometry with source paint retained");
 hand.send(0,0,false,.8f);hand.f.focused=false;hand.send(0,0,false,.8f);settled(xr);check(xr.copy_scene().encode()==xr_authored,"Focus loss cancels an unaccepted controller target stroke exactly");
 hand.f.focused=true;hand.send();hand.send(0,0,false,.8f);hand.send(0,0,true,.8f);settled(xr);
 check(xr.frame().state->menu&&xr.frame().state->menu_page==24&&xr.frame().state->shape_sculpt,"Joystick during a stroke accepts its endpoint and opens the shapekey tools after commit");
 hand.send();hand.sector(11);settled(xr);check(!xr.frame().state->shape_sculpt,"Controller Finish exits the editing preview without editing saved weights");
 std::cout<<"PASS "<<checks<<" native/controller shapekey stroke checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
