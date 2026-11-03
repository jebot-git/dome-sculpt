// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_PAINT_SAMPLING_RENDER_NO_MAIN
#include "paint-sampling-render.cpp"
#undef DOME_PAINT_SAMPLING_RENDER_NO_MAIN
#include "dome/normal-preview.hpp"
int main()try {
 auto source=joint::prepare(true,Material::AlphaMode::Mask);auto current=retain_writable(source);const auto &o=current.active();
 auto legitimate=prepare_normal_preview(*o.painted,o.material_bindings,o.material);check(legitimate&&legitimate->matches(o.material_bindings,o.painted->preview(o.material)),"Actual current owning normal layout has its independent preview proof");
 UvWorkspace workspace(o.painted->mesh(),256);const auto &m=o.painted->mesh();size_t face=0;while(face<m.indices.size()/3&&m.normals[m.indices[3*face]].z<.9f)++face;
 std::array<float,2> p{};for(size_t k=0;k<3;++k){auto q=m.uv[m.indices[3*face+k]];p[0]+=q[0]/3;p[1]+=q[1]/3;}workspace.edit(UvAction::Pick,p[0],p[1]);workspace.edit(UvAction::Scale,.9f);const auto draft=workspace.view();check(draft.valid,"Mismatched-normal fixture uses a valid genuine further UV derivative");
 auto derivative=prepare_polygon_uv_derivative(o,*draft.mesh,*draft.source_vertices,{});auto descriptor=std::make_shared<const PolygonColorSurface>(PolygonColorSurface{std::make_shared<const PolygonMesh>(derivative.authority->topology),derivative.authority->colors});
 auto next=retain_source_corner_paint(o.painted,o.material_bindings,o.deformation,descriptor,derivative.mesh,derivative.source_vertices);
 check(next.paint->mesh().uv!=o.painted->mesh().uv&&next.paint->normal_projection()==o.painted->normal_projection()&&next.paint->baked()==o.painted->baked(),"Actual staged paint derivative retains unchanged normal projection/event while its current authored chart changes");
 auto binding=std::make_shared<SourceMaterialBindings>(*o.material_bindings);binding->color_preparation=next.colors;const auto before=current.encode();
 bool refused=false;try{auto wrong=prepare_normal_preview(*next.paint,binding,o.material);if(wrong)std::cout<<"UNSAFE_PREVIEW_ACCEPTED changed current UV with old normal layout\n";}catch(const std::invalid_argument &e){std::cout<<"REFUSAL "<<e.what()<<'\n';refused=true;}
 check(refused,"Worker preview factory independently rejects the old normal layout on an actual newly authored paint derivative");
 check(current.encode()==before&&legitimate->matches(o.material_bindings,o.painted->preview(o.material)),"Failed intermediate publication leaves the complete original Scene and valid preview exact for retry");
 std::cout<<"PASS "<<checks<<" normal current-owner checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
