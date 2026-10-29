// SPDX-License-Identifier: GPL-3.0-or-later
#include "bdf_font.hpp"
#include "dome/palette.hpp"
#include "dome/bake.hpp"
#include "graphics.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace dome;
int main(int argc, char **argv) try {
  if (argc != 2)
    return 2;
  auto home = std::filesystem::temp_directory_path() /
              ("dome-sculpt-gpu-" + std::to_string(getpid()));
  std::filesystem::create_directory(home);
  std::filesystem::permissions(home, std::filesystem::perms::owner_all);
  setenv("XDG_RUNTIME_DIR", home.c_str(), 1);
  auto config = bdf_fonts::export_config();
  setenv("FONTCONFIG_FILE", config.c_str(), 1);
  auto face = bdf_fonts::resolve("Unifont", "Sans-Serif", 16);
  Document doc;
  doc.seed();
  if (!doc.wait_mesh())
    throw std::runtime_error("No mesh snapshot");
  NativeGraphics graphics;
  SurfaceRenderer renderer;
  SpatialPalette palette;
  GLuint fbo, color, depth;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenTextures(1, &color);
  glBindTexture(GL_TEXTURE_2D, color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color, 0);
  glGenRenderbuffers(1, &depth);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 640, 480);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    throw std::runtime_error("Test framebuffer incomplete");
  auto vp = projection(-.65, .65, -.5, .5);
  CanvasTransform model{{0, 0, -1.8f}, {}, 1};
  auto capture = [&](std::string name) {
    std::vector<uint8_t> bytes(640 * 480 * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, bytes.data());
    std::ofstream out(std::string(argv[1]) + "-" + name + ".ppm",
                      std::ios::binary);
    out << "P6\n640 480\n255\n";
    for (int y = 479; y >= 0; --y)
      out.write(reinterpret_cast<const char *>(bytes.data() + y * 640 * 3),
                640 * 3);
    if (!out)
      throw std::runtime_error("Cannot save GPU capture");
    return bytes;
  };
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480);
  auto pbr = capture("pbr");
  size_t foreground = 0;
  for (size_t i = 0; i < pbr.size(); i += 3)
    if (pbr[i] > 50 && pbr[i] > pbr[i + 2])
      ++foreground;
  if (foreground < 10000)
    throw std::runtime_error("PBR surface render is blank");
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480, false, false,
                true);
  auto light = capture("light");
  if (pbr[0] != 48 || light[0] != 208 || light[0] != light[1] ||
      light[1] != light[2])
    throw std::runtime_error("Appearance does not select grey backgrounds");
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480, true);
  std::array<uint8_t, 4> corner{};
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, corner.data());
  if (corner != std::array<uint8_t, 4>{})
    throw std::runtime_error("XR overlay background must remain transparent");
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480, false, true);
  auto toon = capture("toon");
  if (toon == pbr)
    throw std::runtime_error("Toon preview did not change shading");
  auto state = *doc.frame().state;
  state.menu = true;
  state.menu_index = 1;
  state.font_file = face.file;
  state.font_pixels = face.pixels;
  state.panel.position = {0, 0, -.45f};
  palette.draw(state, vp);
  auto menu = capture("menu");
  size_t blue = 0;
  for (size_t i = 0; i < menu.size(); i += 3)
    if (menu[i] < 150 && menu[i + 1] > 130 && menu[i + 2] > 220)
      ++blue;
  if (blue < 1000)
    throw std::runtime_error("Spatial menu selection is not visible");
  auto slider = state;
  slider.panel.position.z = -.7f;
  slider.controls.parameter = VrParameter::Metallic;
  slider.controls.title = "Material metalness";
  slider.controls.value[0] = .2f;
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480);
  palette.draw(slider, vp);
  auto low_slider = capture("slider-low");
  slider.controls.value[0] = .8f;
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480);
  palette.draw(slider, vp);
  auto high_slider = capture("slider-high");
  size_t changed = 0;
  for (size_t i = 0; i < low_slider.size(); ++i)
    changed += low_slider[i] != high_slider[i];
  if (changed < 1000)
    throw std::runtime_error(
        "Controller slider position is not visibly adjustable");
  auto &pointer = slider.hands[1].tracked;
  pointer.valid = pointer.aimValid = true;
  pointer.aim.position = ob::transform(slider.panel, {.10875f, .01875f, .3f});
  pointer.aim.orientation = slider.panel.orientation;
  CursorRenderer cursors;
  cursors.draw(slider, vp);
  auto pointed_slider = capture("slider-pointer");
  size_t pointer_pixels = 0;
  for (size_t i = 0; i < high_slider.size(); ++i)
    pointer_pixels += high_slider[i] != pointed_slider[i];
  if (pointer_pixels < 10)
    throw std::runtime_error(
        "The controller aim ray and slider reticle are invisible");
  slider.opacity = 64;
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480, true);
  palette.draw(slider, vp);
  std::vector<uint8_t> alpha(640 * 480 * 4);
  glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, alpha.data());
  size_t quarter = 0;
  for (size_t i = 3; i < alpha.size(); i += 4)
    quarter += alpha[i] == 64;
  if (quarter < 40000)
    throw std::runtime_error(
        "XR controls do not preserve DomeD's 25% chrome preset");
  auto original = doc.frame().state->active_object;
  Material red;
  red.color = {.9f, .025f, .01f, 1};
  Material azure;
  azure.color = {.02f, .06f, .9f, 1};
  doc.object_properties(original, "Red", true, false, {{-.28f, 0, 0}, {}, .6f},
                        red);
  auto second = doc.add_object();
  doc.object_properties(second, "Blue", true, false, {{.28f, 0, 0}, {}, .8f},
                        azure);
  if (!doc.wait_mesh())
    throw std::runtime_error("Multi-object materials did not mesh");
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480);
  auto colored = capture("objects");
  size_t reds = 0, blues = 0;
  for (size_t i = 0; i < colored.size(); i += 3) {
    if (colored[i] > colored[i + 2] * 2 && colored[i] > 70)
      ++reds;
    if (colored[i + 2] > colored[i] * 2 && colored[i + 2] > 70)
      ++blues;
  }
  if (reds < 1000 || blues < 1000)
    throw std::runtime_error(
        "Objects did not render with independent PBR colors");
  doc.select_object(original);
  doc.prepare_mesh(128);
  if (!doc.wait_asset() || !doc.frame().state->mesh_active || !doc.wait_mesh())
    throw std::runtime_error("Paint mesh preparation failed");
  doc.begin_uv();
  auto uv_state = slider;
  uv_state.controls.parameter = VrParameter::UvEdit;
  uv_state.uv = doc.frame().state->uv;
  uv_state.opacity = 255;
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480);
  palette.draw(uv_state, vp);
  auto uv_capture = capture("uv-editor");
  size_t edges = 0;
  for (size_t i = 0; i < uv_capture.size(); i += 3)
    edges += uv_capture[i] < 150 && uv_capture[i + 1] > 130 &&
             uv_capture[i + 2] > 200;
  if (edges < 1000)
    throw std::runtime_error(
        "UV chart selection and edit controls are invisible in XR");
  doc.end_uv(false);
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480);
  auto frozen = capture("paint-before");
  PaintSettings ink;
  ink.color = {.01f, .95f, .01f, 1};
  doc.paint_settings(ink);
  Brush brush;
  brush.normal = {0, 0, 1};
  brush.from = brush.to = {-.28f, 0, .22f};
  brush.radius = .24f;
  if (!doc.begin_desktop())
    throw std::runtime_error("Paint transaction unavailable");
  doc.stamp_desktop(brush);
  doc.end_desktop();
  if (!doc.wait_mesh())
    throw std::runtime_error("Paint preview did not update");
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480);
  auto painted = capture("painted");
  size_t greens = 0;
  for (size_t i = 0; i < painted.size(); i += 3)
    if (painted[i + 1] > 70 && painted[i + 1] > painted[i] * 2 &&
        painted[i + 1] > painted[i + 2] * 2)
      ++greens;
  if (greens < 500)
    throw std::runtime_error(
        "Painted PBR texture is not visible in GPU pixels");
  if (!doc.undo() || !doc.wait_mesh())
    throw std::runtime_error("Paint undo did not update");
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480);
  if (capture("paint-undo") != frozen)
    throw std::runtime_error("Paint undo did not restore exact GPU pixels");
  Mesh low;
  low.positions={{-.6f,-.4f,0},{.6f,-.4f,0},{.6f,.4f,0},{-.6f,.4f,0}};
  low.normals={normalized({-.6f,0,.8f}),normalized({.6f,0,.8f}),
                normalized({.6f,.5f,.6f}),normalized({-.6f,.5f,.6f})};
  low.uv={{{.125f,.125f}},{{.875f,.125f}},{{.875f,.875f}},{{.125f,.875f}}};
  low.indices={0,1,2,0,2,3};
  BakeSource high;high.mesh=low;high.mesh.normals.assign(4,normalized({0,.7f,.714f}));
  high.materials[0].material.color={.5f,.5f,.5f,1};
  BakeOptions bake;bake.resolution=64;bake.padding=4;bake.ao_samples=0;
  auto result=bake_mesh(low,high,bake);
  BakedTextures baked;baked.maps=std::move(result.maps);baked.maps.normal=std::move(result.normal);
  for(auto c:result.coverage)baked.coverage.push_back(c==1||c==2?255:0);
  Scene surface;surface.active().material=high.materials[0].material;
  surface.active().mesh_active=true;surface.active().painted=std::make_shared<PaintedMesh>(low,64);
  Document detailed;detailed.replace(surface);
  if(!detailed.wait_mesh())throw std::runtime_error("No curved-normal test surface");
  renderer.draw(detailed.frame().surface,model,vp,{},640,480);
  auto geometric=capture("bake-geometric");
  surface.active().painted->install_bake(std::move(baked));detailed.replace(surface);
  if(!detailed.wait_mesh())throw std::runtime_error("No normal-mapped tangent snapshot");
  renderer.draw(detailed.frame().surface,model,vp,{},640,480);
  auto mapped=capture("bake-normal");
  surface.active().painted=std::make_shared<PaintedMesh>(high.mesh,64);detailed.replace(surface);
  if(!detailed.wait_mesh())throw std::runtime_error("No reference shading surface");
  renderer.draw(detailed.frame().surface,model,vp,{},640,480);
  auto reference=capture("bake-reference");
  size_t errors=0,normal_change=0;
  for(int y=210;y<270;++y)for(int x=270;x<370;++x)for(int c=0;c<3;++c){
    size_t at=(size_t(y)*640+x)*3+c;
    errors+=std::abs(int(mapped[at])-int(reference[at]));
    normal_change+=std::abs(int(mapped[at])-int(geometric[at]));
  }
  if(errors>60*100*3*3 || normal_change<60*100*3*3)
    throw std::runtime_error("Baked MikkTSpace normal does not reconstruct the reference GPU shading");
  std::cout<<"PASS Canonical MikkTSpace baking reconstructs reference shading through real GPU tangent interpolation\n";
  BakedTextures occluded;occluded.maps=*surface.active().painted->composite(surface.active().material);
  occluded.coverage.assign(64*64,255);
  for(size_t i=0;i<occluded.maps.metal_rough.rgba.size();i+=4)occluded.maps.metal_rough.rgba[i]=0;
  surface.active().painted->install_bake(std::move(occluded));detailed.replace(surface);
  if(!detailed.wait_mesh())throw std::runtime_error("No AO shading surface");
  renderer.draw(detailed.frame().surface,model,vp,{},640,480);
  auto shadow=capture("bake-ao");size_t ambient_change=0;
  for(int y=210;y<270;++y)for(int x=270;x<370;++x)for(int c=0;c<3;++c){
    size_t at=(size_t(y)*640+x)*3+c;
    if(shadow[at]>reference[at])throw std::runtime_error("AO increased indirect surface lighting");
    ambient_change+=reference[at]-shadow[at];
  }
  if(ambient_change<60*100*3)throw std::runtime_error("Baked AO is absent from GPU indirect shading");
  std::cout<<"PASS Baked occlusion attenuates indirect GPU shading through the ORM red channel\n";
  auto swatch = std::make_shared<MeshSnapshot>();
  swatch->mesh.positions = {{-.4f,-.4f,0},{.4f,-.4f,0},{.4f,.4f,0},{-.4f,.4f,0}};
  swatch->mesh.normals.assign(4,{0,0,1});
  swatch->mesh.uv = {{{0,0}},{{1,0}},{{1,1}},{{0,1}}};
  swatch->mesh.indices = {0,1,2,0,2,3};
  auto maps = std::make_shared<TextureMaps>();
  maps->color = {1,1,{128,64,192,64}};
  maps->metal_rough = {1,1,{255,255,0,255}};
  maps->emission = {1,1,{0,0,0,255}}; maps->transparent = true;
  Material material; material.color={1,1,1,1}; material.unlit=true;
  material.alpha=Material::AlphaMode::Opaque;
  auto sample = [&](Material m) {
    auto snapshot=std::make_shared<MeshSnapshot>(*swatch);
    snapshot->ranges={{123,0,6,m,maps}};
    renderer.draw(snapshot,model,vp,{},640,480);
    std::array<uint8_t,4> pixel{};
    glReadPixels(320,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    return pixel;
  };
  auto close = [](auto actual,std::array<int,3> expected) {
    for(int i=0;i<3;++i)if(std::abs(int(actual[i])-expected[i])>2)return false;
    return true;
  };
  if(!close(sample(material),{128,64,192}))
    throw std::runtime_error("Unlit source colors were lit/tone-mapped or opaque alpha was blended");
  material.alpha=Material::AlphaMode::Mask; material.alpha_cutoff=.5f;
  if(!close(sample(material),{48,48,48}))throw std::runtime_error("Source alpha mask failed to discard");
  material.alpha_cutoff=.2f;
  if(!close(sample(material),{128,64,192}))throw std::runtime_error("Source alpha mask failed to retain opaque coverage");
  material.alpha=Material::AlphaMode::Blend;
  if(!close(sample(material),{68,52,84}))throw std::runtime_error("Source blend alpha was not preserved");
  glDeleteRenderbuffers(1, &depth);
  glDeleteTextures(1, &color);
  glDeleteFramebuffers(1, &fbo);
  std::filesystem::remove_all(home);
  std::cout << "PASS Native PBR and toon GPU previews, grey appearance "
               "backgrounds, transparent XR overlay, BDF spatial menu and "
               "visible joystick selection\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
