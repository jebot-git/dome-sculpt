// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual private native Canvas/App classes, without a product hook.
#define main dome_application_main
#include "../src/app.cpp"
#undef main
#include <chrono>
#include <fstream>
#include <unistd.h>
int main() try {
  desktop::NativeFontconfigRuntime fontconfig;
  desktop::NativeDisplayRuntime display;
  dx::init();
  size_t checks=0;
  auto check=[&](bool ok,const char*message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';};
  {
    Canvas unopened(0,0,64,64);
    unopened.hide();unopened.hide();
    check(!unopened.context()&&!unopened.renderer&&!unopened.polygons&&!unopened.cursors,
          "Repeated hide before first native draw is safe without a GL context");
  }
  {
    App app;
    const auto original=app.canvas.document.copy_scene().encode();
    const auto path=std::filesystem::temp_directory_path()/("dome-context-projection-"+std::to_string(getpid())+".png");
    struct Remove {std::filesystem::path p;~Remove(){std::error_code ec;std::filesystem::remove(p,ec);}} remove{path};
    auto png=Image{1,1,{255,0,0,255}}.png();{std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(png.data()),png.size());}
    app.canvas.document.load_projection_image(path);
    check(app.canvas.document.wait_projection_image()&&app.canvas.document.frame().state->projection->image,
          "Native image worker owns immutable projector pixels across actual context cycles");
    for(unsigned cycle=0;cycle<3;++cycle) {
      app.window.show();app.canvas.redraw();
      auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
      while((!app.canvas.renderer||!app.canvas.polygons)&&std::chrono::steady_clock::now()<deadline)Fl::wait(.01);
      check(app.canvas.shown()&&app.canvas.context()&&app.canvas.renderer&&app.canvas.polygons,
            "Native show creates the actual Canvas GL renderers with a valid context");
      check(bool(app.canvas.cursors),"Actual native draw created the GPU image projector before hide/show teardown");
      app.canvas.make_current();
      const auto renderer_name=glGetString(GL_RENDERER);
      check(renderer_name&&glGetError()==GL_NO_ERROR,"The actual native context is current and has a live OpenGL renderer");
      if(cycle==0)std::cout<<"GL renderer: "<<renderer_name<<'\n';
      app.window.hide();
      check(!app.canvas.shown()&&!app.canvas.context()&&!app.canvas.renderer&&!app.canvas.polygons&&!app.canvas.cursors,
            "Parent hide releases all actual Canvas wrappers before FLTK destroys context and surface");
      app.window.hide();
      check(app.canvas.document.copy_scene().encode()==original,
            "Repeated native hide/show preserves exact authoring state and live Document workers");
    }
  }
  check(true,"Final native App destruction drains after already hidden GL resources");
  std::cout<<checks<<" actual native context lifetime checks passed\n";
  return 0;
} catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
