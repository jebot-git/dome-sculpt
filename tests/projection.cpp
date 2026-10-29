// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/projection.hpp"
#include "dome/scene.hpp"
#include <atomic>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <unistd.h>
using namespace dome;
namespace {
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
template<class F>bool rejects(F &&f){try{f();return false;}catch(const std::exception&){return true;}}
bool near(float a,float b,float eps=.005f){return std::abs(a-b)<eps;}
Image image(std::array<uint8_t,4> c){return {1,1,{c[0],c[1],c[2],c[3]}};}
Mesh quad(float z=0){Mesh m;m.positions={{-1,-1,z},{1,-1,z},{1,1,z},{-1,1,z}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
void append(Mesh &m,const Mesh &other){auto offset=uint32_t(m.positions.size());m.positions.insert(m.positions.end(),other.positions.begin(),other.positions.end());m.normals.insert(m.normals.end(),other.normals.begin(),other.normals.end());m.uv.insert(m.uv.end(),other.uv.begin(),other.uv.end());for(auto i:other.indices)m.indices.push_back(i+offset);}
Mesh seams(){Mesh m;m.positions={{-1,-1,0},{0,-1,0},{0,1,0},{-1,1,0},{0,-1,0},{1,-1,0},{1,1,0},{0,1,0}};m.normals.assign(8,{0,0,1});m.uv={{{.05f,.05f}},{{.45f,.05f}},{{.45f,.95f}},{{.05f,.95f}},{{.55f,.05f}},{{.95f,.05f}},{{.95f,.95f}},{{.55f,.95f}}};m.indices={0,1,2,0,2,3,4,5,6,4,6,7};return m;}
ProjectionCamera camera(){ProjectionCamera c;c.pose.position={0,0,2};c.width=c.height=2;return c;}
Brush brush(){Brush b;b.from={-.2f,0,0};b.to={.2f,0,0};b.normal={0,0,1};b.radius=.5f;return b;}
struct Temp{std::filesystem::path path;Temp(){std::string p=(std::filesystem::temp_directory_path()/"dome-projection-XXXXXX").string();std::vector<char> n(p.begin(),p.end());n.push_back(0);auto *v=mkdtemp(n.data());if(!v)throw std::runtime_error("Cannot create fixture directory");path=v;}~Temp(){std::error_code ec;std::filesystem::remove_all(path,ec);}};
} // namespace
int main()try{
  auto surface=quad();PaintSettings settings;settings.color={1,1,1,1};
  Image rgba{2,2,{255,0,0,255,0,255,0,128,0,0,255,0,255,255,255,64}};
  auto png=rgba.png();auto decoded=decode_projection_png(png);
  check(decoded.width==2&&decoded.height==2&&decoded.rgba==rgba.rgba,"Native PNG decode preserves original RGBA including transparent pixel data");
  Temp temp;auto file=temp.path/"žluťoučký.png";{std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(png.data()),png.size());}
  check(load_projection_png(file).rgba==rgba.rgba,"Native image loading supports explicit Unicode paths");
  ProjectionImageLimits limits;limits.max_pixels=3;
  check(rejects([&]{decode_projection_png(png,limits);}),"PNG decoded pixel bounds are checked before pixel allocation");
  limits={};limits.max_encoded_bytes=png.size()-1;
  check(rejects([&]{decode_projection_png(png,limits);})&&rejects([&]{load_projection_png(file,limits);}),"Encoded memory and file image bounds are enforced");
  check(rejects([&]{decode_projection_png(std::span<const uint8_t>(png.data(),png.size()/2));}),"Truncated PNG data reports a native decode error");
  mkfifo((temp.path/"image.fifo").c_str(),0600);
  check(rejects([&]{load_projection_png(temp.path/"image.fifo");}),"Image FIFO/device paths are rejected without blocking");

  PaintProjection projector(image({128,128,128,255}),camera(),surface);
  auto sample=projector.sample({0,0,0},{0,0,1},settings);
  check(sample&&near(sample->settings.color[0],srgb_to_linear(128/255.f))&&sample->opacity==1,"Automatic color projection decodes sRGB into linear brush values");
  settings.channel=PaintChannel::Roughness;sample=projector.sample({0,0,0},{0,0,1},settings);
  check(sample&&near(sample->settings.roughness,128/255.f),"Automatic scalar material projection keeps data channels linear");settings.channel=PaintChannel::Color;
  check(!projector.sample({0,0,0},{0,0,-1},settings)&&!projector.sample({2,0,0},{0,0,1},settings)&&!projector.sample({0,0,3},{0,0,1},settings),"Projectors reject back-facing, off-image and behind-camera samples");
  auto clip=camera();clip.far_depth=1;
  check(!PaintProjection(image({255,255,255,255}),clip,surface).sample({0,0,0},{0,0,1},settings),"Camera near/far depths clip in the declared pose units");

  Image edge{2,1,{255,0,0,255,0,0,255,0}};
  auto alpha=PaintProjection(edge,camera(),surface).sample({0,0,0},{0,0,1},settings);
  check(alpha&&near(alpha->opacity,.5f)&&near(alpha->settings.color[0],1)&&alpha->settings.color[2]==0&&alpha->settings.color[3]==1,
        "Premultiplied linear filtering avoids transparent-color fringes and uses alpha as coverage once");
  ProjectionOptions value_mode;value_mode.alpha=ProjectionAlpha::ChannelValue;
  auto value=PaintProjection(edge,camera(),surface,value_mode).sample({0,0,0},{0,0,1},settings);
  check(value&&value->opacity==1&&near(value->settings.color[3],.5f)&&near(value->settings.color[0],.5f)&&near(value->settings.color[2],.5f),
        "Explicit alpha-as-channel mode preserves independent RGBA rather than multiplying alpha twice");
  ProjectionOptions stencil;stencil.usage=ProjectionUsage::Stencil;
  auto linear_stencil=PaintProjection(image({128,128,128,128}),camera(),surface,stencil).sample({0,0,0},{0,0,1},settings);
  check(linear_stencil&&near(linear_stencil->opacity,(128/255.f)*(128/255.f))&&linear_stencil->settings.color==settings.color,
        "Stencil masks preserve brush channel values and interpret automatic mask data linearly");
  stencil.color_space=ProjectionColorSpace::SRGB;
  auto srgb_stencil=PaintProjection(image({128,128,128,128}),camera(),surface,stencil).sample({0,0,0},{0,0,1},settings);
  check(srgb_stencil&&near(srgb_stencil->opacity,srgb_to_linear(128/255.f)*(128/255.f)),"Explicit sRGB stencil luminance is decoded before coverage filtering");
  stencil.scalar=ProjectionScalar::Alpha;
  check(near(PaintProjection(image({0,0,0,128}),camera(),surface,stencil).sample({0,0,0},{0,0,1},settings)->opacity,128/255.f),"Alpha-only stencils never square their alpha coverage");

  Image ramp{2,1,{0,0,0,255,255,255,255,255}};auto perspective=camera();perspective.mode=ProjectionMode::Perspective;perspective.vertical_fov=1.5707963268f;
  auto perspective_projection=PaintProjection(ramp,perspective,surface);
  ProjectionOptions no_visibility;no_visibility.visibility=false;
  auto farther_projection=PaintProjection(ramp,perspective,{},no_visibility);
  auto close=perspective_projection.sample({1,0,0},{0,0,1},settings),farther=farther_projection.sample({1,0,-2},{0,0,1},settings);
  check(close&&farther&&close->settings.color[0]>.99f&&near(farther->settings.color[0],.75f),"Perspective projection scales image position with camera depth");
  auto rotated=camera();rotated.pose.position={2,0,0};rotated.pose.orientation={0,.7071067812f,0,.7071067812f};
  auto side=surface;for(auto &p:side.positions){auto q=ob::rotate(rotated.pose.orientation,{p.x,p.y,p.z});p={q.x,q.y,q.z};}side.normals.assign(4,{1,0,0});
  auto rotation=PaintProjection(ramp,rotated,side);
  check(rotation.sample({0,0,.5f},{1,0,0},settings)->settings.color[0]<.01f&&rotation.sample({0,0,-.5f},{1,0,0},settings)->settings.color[0]>.99f,
        "Right-handed camera pose rotation preserves +X right and -Z forward conventions");
  auto scaled=camera();scaled.width=1;scaled.pose.scale=2;
  check(near(PaintProjection(ramp,scaled,surface).sample({.25f,0,0},{0,0,1},settings)->settings.color[0],.75f),"Orthographic plane dimensions respect the camera's uniform metric scale");
  auto wide=camera();wide.width=4;PaintProjection original_camera(ramp,wide,surface);auto changed_camera=original_camera.with_camera(camera());
  check(near(original_camera.sample({.5f,0,0},{0,0,1},settings)->settings.color[0],.75f)&&changed_camera.sample({.5f,0,0},{0,0,1},settings)->settings.color[0]>.99f,
        "Immutable camera reconfiguration reuses image/BVH and preserves the previous projector");
  ProjectionOptions faded;faded.opacity=.25f;auto changed_opacity=original_camera.with_camera(wide,faded);
  check(near(changed_opacity.sample({0,0,0},{0,0,1},settings)->opacity,.25f)&&original_camera.sample({0,0,0},{0,0,1},settings)->opacity==1,
        "Live projector settings can change without mutating shared source data");
  auto uncached=PaintProjection(ramp,camera(),{},no_visibility);
  check(rejects([&]{uncached.with_camera(camera(),ProjectionOptions{});}),"Enabling uncached visibility requires an explicit asset-worker source rebuild");
  auto flipped=ProjectionOptions{};flipped.uv_transform={-1,0,0,1,1,0};
  check(PaintProjection(ramp,camera(),surface,flipped).sample({.5f,0,0},{0,0,1},settings)->settings.color[0]<.01f,"Stencil UV transforms support explicit image mirroring");

  auto occluded=surface;auto blocker=quad(1);for(auto &p:blocker.positions){p.x*=.4f;p.y*=.4f;}append(occluded,blocker);
  auto visible=PaintProjection(image({255,255,255,255}),camera(),occluded);
  check(!visible.sample({0,0,0},{0,0,1},settings)&&visible.sample({.75f,0,0},{0,0,1},settings),"Cached surface visibility rejects occluded samples while retaining exposed areas");
  no_visibility.front_only=false;
  check(PaintProjection(image({255,255,255,255}),camera(),{},no_visibility).sample({0,0,0},{0,0,-1},settings).has_value(),"Through-surface and backface projection require explicit caller options");

  auto seam_surface=seams();PaintedMesh paint(seam_surface,64);Material material;material.color={.1f,.1f,.1f,0};
  auto baseline=paint.composite(material)->color.rgba;
  check(!paint.has_paint_data(),"An empty painted asset exposes a constant-time empty-data state");
  auto c=camera();c.width=c.height=4;PaintProjection red(image({255,0,0,128}),c,seam_surface);
  auto b=brush();paint.begin_stroke();paint.stamp_projection(b,settings,red);
  check(paint.has_paint_data()&&paint.end_stroke(),"Projected color reuses the existing sparse tile/stroke transaction");
  const auto &left=paint.pixel(paint.active_layer(),28,32),&right=paint.pixel(paint.active_layer(),35,32);
  check(left.data[0]==255&&right.data[0]==255&&left.data[2]==0&&left.data[3]==255&&left.data[9]>110&&left.data[9]<135,
        "Projected image colors remain continuous across independent UV islands with single alpha coverage");
  auto after=paint.composite(material)->color.rgba;
  check(after[(32*64+28)*4+3]>110&&after[(32*64+28)*4+3]<135,"Layer compositing applies projected source alpha once over a transparent material");
  auto held=paint.snapshot();
  check(paint.undo()&&!paint.has_paint_data()&&paint.composite(material)->color.rgba==baseline&&held->has_paint_data()&&held->composite(material)->color.rgba==after,
        "Projected undo restores exact bytes/cache while immutable snapshots retain projected data");
  check(paint.redo()&&paint.has_paint_data()&&paint.composite(material)->color.rgba==after,"Projected redo restores exact original tile identities and data presence");
  paint.layer_properties("Hidden projection",false,0);
  check(paint.has_paint_data()&&PaintedMesh::decode(paint.encode())->has_paint_data(),"Hidden and zero-opacity projected layers remain protected across save/reload");
  auto repacked=seam_surface;for(auto &uv:repacked.uv){uv[0]=uv[0]*.8f+.1f;uv[1]=uv[1]*.8f+.1f;}
  check(paint.reparameterize(repacked)->has_paint_data(),"UV paint transfer retains the constant-time protected-data cache");
  check(rejects([&]{paint.begin_stroke();}),"Projected data does not make hidden layers writable");
  paint.layer_properties("Visible projection",true,1);
  paint.begin_stroke();PaintProjection blue(image({0,0,255,255}),c,seam_surface);paint.stamp_projection(b,settings,blue);paint.cancel_stroke();
  check(paint.has_paint_data()&&paint.composite(material)->color.rgba==after,"Projected stroke cancellation restores exact previous tiles and data state");

  PaintedMesh mask_only(seam_surface,64);PaintSettings mask;mask.channel=PaintChannel::Mask;mask.mask=0;
  mask_only.begin_stroke();mask_only.stamp(b,mask);
  check(mask_only.has_paint_data(),"Mask-only paint is protected before gesture commit");mask_only.cancel_stroke();
  check(!mask_only.has_paint_data(),"Cancelling the first mask stroke restores the empty-data cache");
  mask_only.begin_stroke();mask_only.stamp(b,mask);mask_only.end_stroke();auto mask_id=mask_only.active_layer();mask_only.layer_properties("Hidden mask",false,0);
  check(mask_only.has_paint_data()&&PaintedMesh::decode(mask_only.encode())->has_paint_data(),"Hidden mask-only data remains protected through serialization");
  mask_only.add_layer("Empty layer");check(mask_only.has_paint_data(),"Selecting an empty layer cannot conceal retained data on another layer");
  mask_only.select_layer(mask_id);mask_only.delete_layer();check(!mask_only.has_paint_data(),"Removing the only populated layer updates the constant-time data cache");

  PaintedMesh layer_history(seam_surface,64);layer_history.begin_stroke();layer_history.stamp(b,settings);layer_history.end_stroke();auto first_layer=layer_history.active_layer();
  layer_history.add_layer("Second data");layer_history.begin_stroke();layer_history.stamp(b,settings);layer_history.end_stroke();auto second_layer=layer_history.active_layer();
  layer_history.select_layer(first_layer);layer_history.delete_layer();layer_history.select_layer(second_layer);
  check(layer_history.undo()&&!layer_history.has_paint_data()&&layer_history.redo()&&layer_history.has_paint_data(),
        "Stroke undo adjusts only its own layer data after unrelated layer deletion");

  PaintedMesh scalar_paint(seam_surface,64);PaintSettings scalar_settings;scalar_settings.channel=PaintChannel::Roughness;
  PaintProjection gray(image({128,128,128,255}),c,seam_surface);scalar_paint.begin_stroke();scalar_paint.stamp_projection(b,scalar_settings,gray);scalar_paint.end_stroke();
  check(scalar_paint.pixel(scalar_paint.active_layer(),28,32).data[5]==128,"Linear scalar images remain linear in the existing paint channel storage");
  scalar_settings.channel=PaintChannel::Emission;scalar_paint.begin_stroke();scalar_paint.stamp_projection(b,scalar_settings,gray);scalar_paint.end_stroke();
  check(scalar_paint.pixel(scalar_paint.active_layer(),28,32).data[6]==128,"Projected emission uses sRGB storage with linear compositing values");
  PaintSettings alpha_mask;alpha_mask.channel=PaintChannel::Mask;
  auto alpha_options=ProjectionOptions{};alpha_options.alpha=ProjectionAlpha::ChannelValue;alpha_options.scalar=ProjectionScalar::Alpha;
  PaintProjection alpha_data(image({255,255,255,128}),c,seam_surface,alpha_options);
  scalar_paint.begin_stroke();scalar_paint.stamp_projection(b,alpha_mask,alpha_data);scalar_paint.end_stroke();
  check(std::abs(int(scalar_paint.pixel(scalar_paint.active_layer(),28,32).data[13])-128)<=2,"Alpha-as-data writes masks without unintended duplicate alpha attenuation");

  PaintedMesh rollback(seam_surface,64);ProjectionOptions budget;budget.max_samples_per_stamp=1000;
  PaintProjection limited(image({0,0,255,255}),c,seam_surface,budget);auto whole=b;whole.radius=5;
  rollback.begin_stroke();check(rejects([&]{rollback.stamp_projection(whole,settings,limited);})&&!rollback.has_paint_data()&&!rollback.pending_changes(),
        "Projection sample-budget failure rolls back every already-written tile and cache");rollback.cancel_stroke();
  std::atomic<bool> stop=false;std::atomic<int> visits=0;ProjectionOptions cancellation;
  cancellation.cancelled=[&]{return stop.load()&&++visits>1000;};PaintProjection interrupted(image({0,0,255,255}),c,seam_surface,cancellation);
  rollback.begin_stroke();rollback.stamp(b,settings);auto before_failure=rollback.composite(material)->color.rgba;stop=true;
  check(rejects([&]{rollback.stamp_projection(whole,settings,interrupted);})&&rollback.has_paint_data()&&rollback.composite(material)->color.rgba==before_failure,
        "Mid-projection cancellation preserves earlier gesture stamps exactly");rollback.cancel_stroke();
  check(!rollback.has_paint_data()&&rollback.undo_count()==0,"Cancelling the complete mixed gesture restores its original empty history");

  Image captured=image({255,0,0,255});auto captured_surface=surface;PaintProjection immutable(captured,camera(),captured_surface);
  captured.rgba={0,0,255,255};for(auto &p:captured_surface.positions)p.z+=1;
  auto retained=immutable.sample({0,0,0},{0,0,1},settings);
  check(retained&&retained->settings.color[0]==1&&retained->settings.color[2]==0,"Projection image and visibility geometry are immutable source snapshots");
  auto invalid=camera();invalid.width=0;
  check(rejects([&]{PaintProjection bad(image({1,2,3,4}),invalid,surface);}),"Invalid metric projector dimensions fail before sampling");
  auto singular=ProjectionOptions{};singular.uv_transform={0,0,0,0,0,0};
  check(rejects([&]{PaintProjection bad(image({1,2,3,4}),camera(),surface,singular);}),"Singular image transforms cannot enter paint transactions");
  check(rejects([&]{projector.sample({0,0,0},{0,0,0},settings);}),"Invalid source surface normals fail explicitly");
  std::cout<<"Native image projection tests complete\n";
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
