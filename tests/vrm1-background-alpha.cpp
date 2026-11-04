// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/vrm-expression.hpp"
#include "dome/material-visibility.hpp"
#include "dome/material-bindings.hpp"
#include "dome/humanoid-creator.hpp"
#include <iostream>
using namespace dome;
int main(int argc,char **argv)try{
 if(argc!=2)return 2;Scene baseline=Scene::load(argv[1]);const auto original=baseline.encode();const auto id=baseline.active().id;Vrm1ExpressionWeights weights;weights.preset["happy"]=1;auto current=bake_vrm1_expression_pose(baseline,id,weights);auto &object=current.object(id);object.material.alpha=Material::AlphaMode::Mask;object.material.alpha_cutoff=.5f;
 // This control hides genuine layers through their real API, retaining every
 // painted tile. It isolates the original background factor from coverage.
 const auto held=object.painted;auto paint=held->snapshot();for(const auto &layer:held->layers()){paint->select_layer(layer.id);paint->layer_properties(layer.name,false,layer.opacity);}object.painted=paint;
 for(size_t i=0;i<held->layers().size();++i)if(held->layers()[i].tiles!=paint->layers()[i].tiles||held->layers()[i].emission_tiles!=paint->layers()[i].emission_tiles)throw std::runtime_error("Hiding layer changed its actual paint owners");
 Document doc;doc.replace(current.snapshot());doc.shading(false);if(!doc.wait_mesh()||!doc.frame().state->error.empty())throw std::runtime_error("Actual current PBR frame failed: "+doc.frame().state->error);const auto frame=doc.frame();if(!frame.state->pbr_view||!frame.surface->pbr_view||frame.surface->ranges.size()!=1||!frame.surface->ranges[0].pbr_view||!frame.surface->picking)throw std::runtime_error("Actual PBR immutable query context is missing");
 const auto &a=*object.appearance;const ImportedPrimitive *source=nullptr;for(const auto &p:a.asset->primitives)if(p.source_mesh==a.source_mesh&&p.source_primitive==a.source_primitive){if(source)throw std::runtime_error("Ambiguous actual source primitive");source=&p;}if(!source||source->colors.empty())throw std::runtime_error("Actual authored COLOR_0 alpha witness missing");
 const auto &image=*object.painted->source_maps()->color.image;for(size_t i=3;i<image.rgba.size();i+=4)if(image.rgba[i]!=255)throw std::runtime_error("Isolated source-alpha fixture is not genuinely opaque");
 const float factor=object.material_bindings->material.parameters.base_color[3];if(factor!=.7f)throw std::runtime_error("Actual static expression target alpha not admitted");const auto &mesh=frame.surface->mesh;const auto &rest=object.painted->mesh();size_t tested=0,mismatched=0;
 for(size_t triangle=0;triangle<rest.indices.size()/3;++triangle)for(int x=1;x<9;++x)for(int y=1;y<10-x;++y){const std::array<float,3> bary{float(x)/10,float(y)/10,float(10-x-y)/10};float vertex_alpha=0;Vec3 point{};
  for(size_t c=0;c<3;++c){const auto raw=a.source_vertices.at(rest.indices.at(triangle*3+c));vertex_alpha+=source->colors.at(raw)[3]*bary[c];point+=mesh.positions.at(mesh.indices.at(triangle*3+c))*bary[c];}
  const auto v0=mesh.positions.at(mesh.indices.at(triangle*3)),v1=mesh.positions.at(mesh.indices.at(triangle*3+1)),v2=mesh.positions.at(mesh.indices.at(triangle*3+2));const auto normal=normalized(cross(v1-v0,v2-v0));const bool expected=double(factor)*vertex_alpha>=.5;const bool actual=bool(frame.surface->picking->raycast(point+normal,normal*-1,2));++tested;if(expected!=actual){++mismatched;if(mismatched<4)std::cout<<"ALPHA_MISMATCH factor="<<factor<<" vertex="<<vertex_alpha<<" expected="<<expected<<" actual="<<actual<<'\n';}
 }
 if(doc.copy_scene().encode()!=current.encode()||baseline.encode()!=original)throw std::runtime_error("Actual PBR query mutated native source/paint state");std::cout<<"PBR_ALPHA tested="<<tested<<" mismatched="<<mismatched<<'\n';if(!tested||mismatched)throw std::runtime_error("Native current VRM1 CorePBR alpha does not use its actual shared source factor");std::cout<<"PASS actual current-V1 background alpha and retained hidden paint\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
