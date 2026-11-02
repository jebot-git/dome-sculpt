// SPDX-License-Identifier: GPL-3.0-or-later
#define main rig_fit_authoring_fixture_main
#include "rig-fit-authoring.cpp"
#undef main
#include "import-controller-driver.hpp"
#include <unistd.h>
int main()try{
 const auto root=std::filesystem::temp_directory_path()/("dome-rig-fit-controller-"+std::to_string(getpid()));std::filesystem::create_directories(root);setenv("HOME",root.c_str(),1);setenv("XDG_STATE_HOME",root.c_str(),1);setenv("XDG_CONFIG_HOME",root.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{root};
 auto original=fixture();std::array<double,16> a{-1.3,0,0,0,.35,.8,0,0,.1,.15,1.2,0,.15,.1,-.2,1};original.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(a));
 Document doc;doc.replace(original);check(doc.wait_mesh(),"Rig fitting controller gets exact immutable original preview");ImportDriver driver(doc);driver.open();for(int sector:{8,10,5,7}){driver.sector(sector);auto s=doc.frame().state;std::cout<<"DIAG sector "<<sector<<" page "<<s->menu_page<<" menu "<<s->menu<<" fit "<<bool(s->rig_fit)<<" error "<<s->error<<'\n';}
 check(doc.frame().state->menu_page==27&&doc.frame().state->rig_fit,"Actual joystick opens Objects / Humanoids / Rig / Humanoid fitting");
 auto view=*doc.frame().state->rig_fit;view.landmarks=marks();view.role=0;doc.configure_rig_fit(view);const auto before=doc.copy_scene().encode();
 driver.sector(3);check(doc.frame().state->rig_fit->settings,"Actual joystick opens shared fitting options");auto step=doc.frame().state->rig_fit->step;driver.sector(5);check(doc.frame().state->rig_fit->step>step,"Actual joystick changes world-metric landmark adjustment step");driver.sector(3);view=*doc.frame().state->rig_fit;
 const auto role=human_roles()[0].name;const auto initial=doc.frame().state->rig_fit->landmarks.points.at(role);driver.sector(2);
 check(doc.frame().state->rig_fit->placing&&!doc.frame().state->menu,"Landmark placement owns fresh trigger separately from painting");
 const auto world=original.world_matrix(1);const auto desired=initial+Vec3{.01f,0,0};const auto placed=p(world,desired);driver.frame.hands[1].tracked.grip=ob::compose(doc.frame().state->canvas,CanvasTransform{{placed.x,placed.y,placed.z},{},1});driver.send(0,0,false,.8f);driver.send();
 check(doc.frame().state->rig_fit&&!doc.frame().state->rig_fit->placing&&length(doc.frame().state->rig_fit->landmarks.points.at(role)-desired)<1e-5f,"Actual trigger maps world controller location through inverse reflected/sheared object matrix");
 check(doc.copy_scene().encode()==before&&!doc.frame().state->can_undo,"Landmark guide placement changes no painting source or global history");
 const auto point_before=doc.frame().state->rig_fit->landmarks.points.at(role);driver.sector(5);const auto after=doc.frame().state->rig_fit->landmarks.points.at(role);const auto delta=p(world,after)-p(world,point_before);
 check(length(delta-Vec3{view.step,0,0})<1e-5f,"Shared axis buttons move by exact world metric under reflection and shear");
 driver.sector(2);driver.send(0,0,false,.8f);driver.frame.focused=false;driver.send(0,0,false,.8f);
 check(doc.frame().state->rig_fit&&!doc.frame().state->rig_fit->placing&&length(doc.frame().state->rig_fit->landmarks.points.at(role)-after)<1e-5f,"Actual focus loss cancels and restores prior landmark exactly");
 driver.frame.focused=true;driver.send();view=*doc.frame().state->rig_fit;view.landmarks=marks();doc.configure_rig_fit(view);driver.open();driver.sector(8);driver.sector(10);driver.sector(5);driver.sector(7);driver.sector(10);
 const auto file=root/"fitted.dsculpt";auto saved=doc.save_async(file);saved.get();check(doc.wait_asset()&&doc.frame().state->error.empty(),"Controller Apply executes fitting on the existing ordered worker");
 const auto accepted=doc.copy_scene().encode();check(doc.copy_scene().active().deformation->skin&&Scene::load(file).encode()==accepted,"Controller acceptance plus immediate Save/restart retains actual weights and landmarks");check(doc.undo()&&doc.copy_scene().encode()==before,"Actual controller fit is one exact global Undo");check(doc.redo()&&doc.copy_scene().encode()==accepted,"Actual controller fit Redo retains complete source and fitted data");
 doc.begin_rig_fit();auto stale_view=*doc.frame().state->rig_fit;doc.cancel_rig_fit();doc.replace(original);doc.begin_rig_fit();auto current_view=doc.frame().state->rig_fit;bool refused=false;try{doc.configure_rig_fit(stale_view);}catch(...){refused=true;}check(refused&&doc.frame().state->rig_fit==current_view,"Stale dialog cannot overwrite a newly opened fitting source");doc.cancel_rig_fit({stale_view.generation,stale_view.revision,stale_view.object,stale_view.edit_authority});check(doc.frame().state->rig_fit==current_view,"Stale dialog cancellation cannot close another source tool");doc.cancel_rig_fit();check(doc.copy_scene().encode()==before,"Cancelling shared fitting tools retains exact original Scene");
 std::cout<<checks<<" humanoid fitting controller checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
