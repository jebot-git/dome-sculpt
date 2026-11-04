// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NATIVE_PHYSICAL_CONTRACTS_NO_MAIN
#include "native-physical-contracts.cpp"
#undef DOME_NATIVE_PHYSICAL_CONTRACTS_NO_MAIN
#include "color-bound-memo.hpp"
namespace {
void empty_context(){check(detail::color_bound_context().count==0&&detail::color_bound_context().depth==0,"Color numerical memo releases every owner on top-level return/exception");}
}
int main(int argc,char**argv)try {
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 auto source=joint::transfer(prepared64());ink(source);const auto exact=source.encode();auto stats=detail::last_color_bound_memo_stats();
 std::cout<<"COLOR_MEMO_ENCODE hits="<<stats.hits<<" misses="<<stats.misses<<std::endl;
 check(stats.hits>0,"Real retained Scene encode reuses a computed bound for an exact owning field context");empty_context();check(source.encode()==exact,"Per-call numerical memo preserves exact complete Scene bytes");empty_context();
 const auto node=source.active().material_bindings->color_preparation->paint_sampling;
 const auto recipe=node->before_material()->color_preparation;const auto &mesh=node->before_paint()->mesh();
 {detail::ColorBoundScope call;recipe->validate_current(mesh);const auto hits=detail::color_bound_context().stats.hits;recipe->validate_current(mesh);
  check(detail::color_bound_context().stats.hits>hits,"Repeated current bound uses actual private COW geometry and field/image owners");}
 empty_context();
 // Same recipe address, same image address, different control block: recompute.
 {auto copy=*recipe;detail::ColorBoundScope call;copy.validate_current(mesh);const auto misses=detail::color_bound_context().stats.misses;
  copy.projection=std::shared_ptr<const PaintColorProjection>(recipe->projection.get(),[](const PaintColorProjection*){});copy.validate_current(mesh);
  check(detail::color_bound_context().stats.misses>misses,"Foreign control block cannot borrow the original projection-owner memo");}
 empty_context();
 {Mesh borrowed=mesh;detail::ColorBoundScope call;const auto before=detail::color_bound_context().stats;recipe->validate_current(borrowed);recipe->validate_current(borrowed);
  check(detail::color_bound_context().stats.hits==before.hits,"Equal-valued borrowed geometry recomputes instead of impersonating the actual COW owner");}
 empty_context();
 // A legal mutable image alias is detected even inside one explicit call scope.
 {auto copy=*recipe;auto image=std::make_shared<Image>(*recipe->projection->vertex_rgba);auto projected=std::make_shared<PaintColorProjection>(*recipe->projection);projected->vertex_rgba=image;copy.projection=projected;
  detail::ColorBoundScope call;copy.validate_current(mesh);std::fill(image->rgba.begin(),image->rgba.end(),0);
  check(refusal([&]{copy.validate_current(mesh);},"recorded source-field"),"Same-owner mutable pixels cannot reuse a stale numerical bound");}
 empty_context();
 {auto copy=*recipe;auto descriptor=std::make_shared<PolygonColorSurface>(*recipe->polygon_colors);
  auto field=std::make_shared<PolygonColorField>(*descriptor->field);descriptor->field=field;copy.polygon_colors=descriptor;
  detail::ColorBoundScope call;copy.validate_current(mesh);auto rows=field->sources();for(auto &row:rows)row.value={0,0,0,1};
  *field=PolygonColorField::from_corners(*descriptor->topology,rows);
  check(refusal([&]{copy.validate_current(mesh);},"recorded source-field"),"Same-owner mutable corner values recompute instead of borrowing old field quality");}
 empty_context();
 {auto copy=*recipe;auto projected=std::make_shared<PaintColorProjection>(*recipe->projection);copy.projection=projected;detail::ColorBoundScope call;copy.validate_current(mesh);++projected->quality_cells;
  check(refusal([&]{copy.validate_current(mesh);},"quality work"),"Current claimed quality scalar is checked again instead of memoizing validation success");}
 empty_context();
 {std::vector<SourceColorPreparation> copies(70,*recipe);detail::ColorBoundScope call;for(auto &copy:copies)copy.validate_current(mesh);
  check(detail::color_bound_context().count<=64&&detail::color_bound_context().stats.full_fallback>0,"Full fixed-capacity memo falls back to ordinary computation without admitting or rejecting extra nodes");}
 empty_context();
 {bool caught=false;try{detail::ColorBoundScope call;recipe->validate_current(mesh);const auto hits=detail::color_bound_context().stats.hits;
   recipe->validate_current(mesh,nullptr,[&]{return detail::color_bound_context().stats.hits>hits;});
  }catch(const std::exception&e){caught=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}
  check(caught,"A real cached-bound hit still polls and honors cancellation");}
 empty_context();recipe->validate_current(mesh);check(source.encode()==exact,"Exception/cancellation retry leaves every original source and native field exact");empty_context();
 {auto colors=detail::polygon_target_colors(mesh,*recipe->current_descriptor());detail::ColorBoundScope call;
  check(refusal([&]{detail::color_projection_bound(mesh,colors,*recipe->projection->vertex_rgba,1);},"budget"),"Smaller actual work limits retain ordinary quality-resource refusal");}
 empty_context();
 size_t baseline=0;auto exercise=[&]{detail::ColorBoundScope call;recipe->validate_current(mesh);recipe->validate_current(mesh);};
 {allocation_probe::Guard guard(SIZE_MAX);exercise();baseline=allocation_probe::calls;}
 size_t denied=0;bool success=false;
 for(size_t prefix=0;prefix<=baseline+128;++prefix){bool failed=false;{allocation_probe::Guard guard(prefix);try{exercise();}catch(const std::bad_alloc&){failed=true;}}
  if(failed){check(detail::color_bound_context().count==0&&detail::color_bound_context().depth==0&&source.encode()==exact,"Every persistent new-entry/hit allocation denial clears its memo and preserves exact whole native state");++denied;}
  else{success=true;break;}}
 check(success&&denied>0&&source.encode()==exact,"All actual numerical-memo allocation prefixes preserve exact native state and retry");
 std::cout<<"COLOR_MEMO_PERSISTENT_PREFIXES "<<denied<<" baseline="<<baseline<<std::endl;
 source.save(out/"source.dsculpt");export_glb_authoring(owner(source),out/"native.glb");check(Scene::load(out/"native.glb").encode()==exact,"Memoized actual native production recovers the exact complete project");
 std::cout<<"PASS "<<checks<<" color-bound memo controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
