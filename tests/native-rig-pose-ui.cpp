// SPDX-License-Identifier: GPL-3.0-or-later
#define main dome_sculpt_original_main
#include "../src/app.cpp"
#undef main
#include <FL/Fl_Choice.H>
#include <FL/Fl_Value_Input.H>
namespace {
struct Observer {
 App&app;std::filesystem::path file;uint64_t sequence=0;
 void widgets(Fl_Widget*w,nlohmann::json&out){
  nlohmann::json j={{"label",w->label()?w->label():""},{"x",w->x()},{"y",w->y()},{"w",w->w()},{"h",w->h()},{"active",bool(w->active())},{"visible",bool(w->visible())}};
  if(auto*c=dynamic_cast<Fl_Choice*>(w)){j["kind"]="choice";j["value"]=c->value();j["text"]=c->text()?c->text():"";out.push_back(j);}
  else if(auto*v=dynamic_cast<Fl_Value_Input*>(w)){j["kind"]="value";j["value"]=v->value();out.push_back(j);}
  else if(dynamic_cast<Fl_Box*>(w)){j["kind"]="box";out.push_back(j);}
  if(auto*g=dynamic_cast<Fl_Group*>(w))for(int n=0;n<g->children();++n)widgets(g->child(n),out);
 }
 static void tick(void*opaque){auto&s=*static_cast<Observer*>(opaque);auto state=s.app.canvas.document.frame().state;auto windows=nlohmann::json::array();
  for(auto*w=Fl::first_window();w;w=Fl::next_window(w))if(w->shown()){auto fields=nlohmann::json::array();s.widgets(w,fields);windows.push_back({{"title",w->label()?w->label():""},{"x",w->x()},{"y",w->y()},{"w",w->w()},{"h",w->h()},{"widgets",fields}});}
  std::ofstream(s.file)<<nlohmann::json{{"sequence",++s.sequence},{"saving",bool(s.app.saving)},{"queued",s.app.native_commands.size()},{"busy",state->asset_busy},{"undo",state->can_undo},{"redo",state->can_redo},{"error",state->error},{"dirty",state->dirty},{"active",state->active_object},{"windows",windows}}.dump();Fl::repeat_timeout(.03,tick,opaque);
 }
 Observer(App&a,std::filesystem::path p):app(a),file(std::move(p)){Fl::add_timeout(.03,tick,this);}~Observer(){Fl::remove_timeout(tick,this);}
};
}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::invalid_argument("Choose actual existing project and read-only observer path");Fl_Window::default_xclass("org.domed.Sculpt");dx::init();App app;app.canvas.document.open(argv[1]);Observer observer(app,argv[2]);app.window.show();return Fl::run();
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
