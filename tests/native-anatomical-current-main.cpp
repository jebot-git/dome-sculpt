// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-anatomical-weights.hpp"
#include "dome/bake-recipe.hpp"
#include <iostream>
using namespace dome;using namespace dome::detail;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::invalid_argument("authentic scene");auto s=Scene::load(argv[1]);uint64_t ref=0,target=0;for(auto&o:s.objects())if(o.deformation&&o.deformation->skeleton)ref=o.id;for(auto&o:s.objects())if(o.id!=ref&&o.mesh_active&&o.painted&&o.appearance){target=o.id;break;}
 if(!ref||!target)throw std::runtime_error("Missing genuine native input");
 auto check=[](bool v,const char*name){if(!v)throw std::runtime_error(name);std::cout<<"PASS "<<name<<std::endl;};
 auto recipe=std::make_shared<ObjectBakeRecipe>();recipe->source={target,target,std::string(128,'s')};recipe->cage=BakeObjectReference{target,target,std::string(128,'c')};recipe->source.name.reserve(2048);recipe->cage->name.reserve(4096);recipe->settings.cage_object=target;recipe->validate();
 native_weight_retention_impl::Counter c(1024*1024);auto initial=c.used;c.bake_recipe(recipe);auto retained=c.used;
 check(retained-initial>=sizeof(ObjectBakeRecipe)+recipe->source.name.capacity()+recipe->cage->name.capacity(),"Complete current recipe and both retained dependency names charged");c.bake_recipe(recipe);check(c.used==retained,"Exact shared recipe object/control charged once");
 auto r=capture_native_anatomical(s,std::span<const uint64_t>(&target,1),ref);s.object(target).bake_recipe=recipe;check(!r.matches(s),"Changed current recipe owner invalidates complete source lease");
 auto r2=capture_native_anatomical(s,std::span<const uint64_t>(&target,1),ref);check(r2.matches(s),"Current-main recipe-owning request captures exact source");s.object(target).bake_recipe=std::make_shared<ObjectBakeRecipe>(*recipe);check(!r2.matches(s),"Equal recipe values in a foreign owner cannot borrow the original lease");
 auto origin=std::make_shared<PolygonOrigin>(*capture_polygon_origin(s,target));origin->bake_recipe=recipe;native_weight_retention_impl::Counter before(256u*1024u*1024u);before.origin(origin);auto with=before.used;origin->bake_recipe.reset();native_weight_retention_impl::Counter after(256u*1024u*1024u);after.origin(origin);check(with-after.used>=retained-initial,"Capsule-only bake recipes join owning resource closure");
 auto b=s.object(target).material_bindings;if(b){auto d=std::make_shared<SourceMaterialBindings>(*b); // no fake derivation witness is minted
  check(!d->key_derivation,"Authentic simple source has no inferred derived-key authority");}
 std::cout<<"Current-main anatomical recipe controls complete"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
