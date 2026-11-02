// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-target-fixture.hpp"
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/uv.hpp"
#include <iostream>
using namespace dome;
int main()try{
 auto scene=current_color_target_fixture::fixture(true);Document doc;doc.replace(scene);ObjectBakeSettings options;options.prepare_colors=true;options.ao_samples=0;doc.bake_from_object(scene.objects().back().id,options);
 if(!doc.wait_asset()||!doc.wait_mesh()||!doc.frame().state->error.empty())throw std::runtime_error("Domain fixture actual Bake failed");scene=doc.copy_scene();auto &source=scene.active();
 PaintImageArchive images;auto encoded=source.painted->encode(&images);PaintSourceMaps maps;maps.color.image=std::make_shared<const Image>(Image{2,2,{255,30,60,255,20,240,70,255,40,70,255,255,240,220,50,255}});maps.color.texcoord=1;maps.uv1=std::make_shared<const std::vector<std::array<float,2>>>(source.painted->mesh().uv);maps.validate_surface(source.painted->mesh());encoded["source_maps"]=encode_source_maps(maps,images);source.painted=PaintedMesh::decode(encoded,&images);scene.validate();const auto before=scene.encode();
 UvWorkspace workspace(source.painted->mesh(),64);workspace.edit(UvAction::Move,.01f,0);auto view=workspace.view();auto derivative=prepare_polygon_uv_derivative(source,*view.mesh,*view.source_vertices,{});
 SourceUvBakeInput input;input.paint=source.painted;input.current_material=source.material_bindings;input.current_deformation=source.deformation;input.polygon_authority=source.polygon;input.target_polygon=derivative.authority;input.target=derivative.mesh;input.source_vertices=derivative.source_vertices;
 bool refused=false;try{bake_source_uv(input);}catch(const std::invalid_argument &e){refused=std::string_view(e.what()).find("UV1")!=std::string_view::npos;}
 if(!refused)throw std::runtime_error("Current-corner UV1 source was accepted without its qualified current-field domain adapter");
 if(scene.encode()!=before||input.paint!=source.painted)throw std::runtime_error("Domain refusal changed exact caller paint/source");std::cout<<"PASS actual paired UV1 input remains explicit refused without changing source/current corner authority\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
