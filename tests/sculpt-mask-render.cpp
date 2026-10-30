// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_reference_main
#include "mtoon-render.cpp"
#undef main
#include <limits>
namespace {
bool same_alpha(const Pixels &a,const Pixels &b) {
  if(a.size()!=b.size())return false;
  for(size_t i=3;i<a.size();i+=4)if(a[i]!=b[i])return false;
  return true;
}
std::shared_ptr<MeshSnapshot> mask(std::shared_ptr<const MeshSnapshot> source,float value) {
  auto result=std::make_shared<MeshSnapshot>(*source);
  result->sculpt_masks.assign(result->mesh.positions.size(),value);return result;
}
}
int main(int argc,char **argv)try {
  NativeGraphics graphics;
  std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GPU "<<device<<'\n';
  check(argc<2||std::string(argv[1])!="--hardware"||
        (device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),
        "Sculpt mask fixture executes on requested GPU hardware");
  Fbo fbo;fbo.bind();SurfaceRenderer renderer;
  renderer.set_output_encoding(SurfaceOutputEncoding::Linear);
  auto unlit=snapshot();
  auto ordinary=draw(renderer,unlit);
  check(close(at(ordinary),{255,255,255,255},1),"Independent white unlit reference reaches a linear framebuffer");
  check(close(at(draw(renderer,mask(unlit,.5f))),{163,163,163,255},1),"Half protection darkens in linear light before display transfer");
  check(close(at(draw(renderer,mask(unlit,1))),{71,71,71,255},1),"Full protection remains visible with unchanged opacity");
  renderer.set_output_encoding(SurfaceOutputEncoding::Srgb);
  for(int mode=0;mode<4;++mode) {
    auto scene=snapshot();
    if(mode==1)scene->ranges[0].material.unlit=false;
    if(mode>=2) {
      auto mt=typed(mode==2);mt.parameters.base_color={.7f,.4f,.2f,1};
      scene->ranges[0].appearance=appearance(mt);
    }
    auto baseline=draw(renderer,scene);
    check(draw(renderer,mask(scene,0))==baseline,"Zero protection leaves the entire PBR/MToon image bit-exact");
    check(draw(renderer,mask(scene,-1))==baseline,"Ordinary-object sentinel leaves the entire PBR/MToon image bit-exact");
    auto protected_pixels=draw(renderer,mask(scene,1));
    check(same_alpha(baseline,protected_pixels)&&at(protected_pixels)[0]<at(baseline)[0]-15,
          "PBR and both MToon versions show protection without changing silhouette or coverage");
    scene->selected_weights.assign(4,.5f);
    baseline=draw(renderer,std::make_shared<MeshSnapshot>(*scene));
    check(draw(renderer,mask(scene,1))==baseline,"Joint-weight preview takes priority over a sculpt-mask overlay");
  }
  auto cutout=snapshot();cutout->ranges[0].material.alpha=Material::AlphaMode::Mask;
  auto maps=std::make_shared<PaintSourceMaps>();
  maps->color.image=std::make_shared<const Image>(Image{2,1,{255,255,255,0,255,255,255,255}});
  maps->color.min_filter=maps->color.mag_filter=GL_NEAREST;
  auto painted=PaintedMesh::from_maps(quad(),maps,64);
  cutout->ranges[0].textures=painted->preview(cutout->ranges[0].material);
  auto reference=draw(renderer,cutout);auto stats=renderer.texture_stats();
  auto protected_scene=mask(cutout,1);auto pixels=draw(renderer,protected_scene);
  MaterialSurfaceQuery query(protected_scene->mesh,protected_scene->ranges);
  check(same_alpha(reference,pixels)&&at(pixels,36,48)[3]==0&&at(pixels,60,48)[3]==255,
        "Mask overlay preserves source-texture cutout holes and surviving fragments");
  check(!query.raycast({-.25f,0,2},{0,0,-1},4)&&query.raycast({.25f,0,2},{0,0,-1},4),
        "Overlay opacity matches independent material-aware surface picking");
  check(renderer.texture_stats().uploads==stats.uploads,"Changing only protection does not upload or read back material textures");
  auto blended=snapshot();blended->ranges[0].material.alpha=Material::AlphaMode::Blend;
  blended->ranges[0].material.color[3]=.4f;
  auto before=draw(renderer,blended);auto after=draw(renderer,mask(blended,1));
  check(same_alpha(before,after)&&at(after)[3]>0&&at(after)[3]<255,"Protection preserves actual blended alpha");
  for(float value:{std::numeric_limits<float>::quiet_NaN(),-2.f,-.1f,1.1f}) {
    check(rejects([&]{draw(renderer,mask(unlit,value));}),"Invalid mask scalars reject before GPU publication");
    check(draw(renderer,unlit)==ordinary,"Rejected overlay leaves valid geometry and materials reusable");
  }
  auto short_mask=mask(unlit,1);short_mask->sculpt_masks.pop_back();
  check(rejects([&]{draw(renderer,short_mask);}),"Mismatched mask and packed-vertex counts reject");
  CursorRenderer cursor;EditorState state;
  state.radius=.08f;state.active_object=1;
  state.objects.push_back({1,"Source",true,false,{},white(),false,false});
  state.hands[0].tracked.valid=true;state.hands[0].tracked.grip.position={.3f,.3f,0};
  auto cursors=[&] {
    fbo.bind();glClearColor(0,0,0,0);glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);cursor.draw(state,identity);return read();
  };
  auto count_region=[](const Pixels &pixels,int x0,int y0,int x1,int y1) {
    size_t count=0;for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)count+=at(pixels,x,y)[3]!=0;return count;
  };
  auto plain_cursor=cursors();
  check(count_region(plain_cursor,57,57,68,68)>0&&count_region(plain_cursor,28,57,39,68)==0,
        "Ordinary brush feedback appears only at the tracked hand");
  state.sculpt_settings.symmetry_axes=1;
  auto mirrored_cursor=cursors();
  check(count_region(mirrored_cursor,28,57,39,68)>0,
        "Object-local X symmetry displays the reflected brush cursor");
  state.sculpt_transform.orientation={0,0,std::sqrt(.5f),std::sqrt(.5f)};
  state.sculpt_transform.position={.1f,0,0};
  auto rotated_cursor=cursors();
  check(count_region(rotated_cursor,57,28,68,39)>0&&count_region(rotated_cursor,28,57,39,68)==0,
        "Rotated translated object symmetry follows its local plane instead of world X");
  state.mesh_active=true;
  check(cursors()==plain_cursor,"Paint mode does not display unrelated sculpt symmetry");
  check(glGetError()==GL_NO_ERROR,"Mask rendering and rejection leave the GPU error queue clean");
  std::cout<<"Sculpt mask GPU checks complete: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
