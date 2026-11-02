// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_normal_colour_authoring_suite
#include "normal-color-uv.cpp"
#undef main
#include <limits>
namespace dag {
thread_local bool armed=false;thread_local size_t calls=0;
void visit(const std::shared_ptr<const SourceMaterialBindings> &b,std::set<const SourceMaterialBindings*> &seen){
 if(!b||!seen.insert(b.get()).second)return;
 if(auto p=b->color_preparation){visit(p->original_material,seen);if(p->uv_reprojection)visit(p->uv_reprojection->before_material,seen);}
 if(auto p=b->normal_preparation){visit(p->original_material,seen);if(p->uv_reprojection)visit(p->uv_reprojection->before_material,seen);if(p->sampling_domain){visit(p->sampling_domain->before_material,seen);visit(p->sampling_domain->sampling_material,seen);}}
}
}
extern "C" void __real__ZNK4dome22SourceMaterialBindings8validateEb(const SourceMaterialBindings*,bool);
extern "C" void __wrap__ZNK4dome22SourceMaterialBindings8validateEb(const SourceMaterialBindings *p,bool deep){if(dag::armed)++dag::calls;__real__ZNK4dome22SourceMaterialBindings8validateEb(p,deep);}
int main(int argc,char **argv)try{
 auto scene=argc==2?Scene::load(argv[1]):transfer(transfer(transfer(prepared(false),UvAction::Scale,.7f),UvAction::Rotate,1.57079632679f),UvAction::Conformal,0);
 auto holder=scene.active().material_bindings;std::set<const SourceMaterialBindings*> nodes;dag::visit(holder,nodes);check(nodes.size()>=5,"Fixture owns genuine shared colour/normal predecessor branches");
 for(bool deep:{false,true}){dag::calls=0;dag::armed=true;try{holder->validate(deep);}catch(...){dag::armed=false;throw;}dag::armed=false;std::cout<<"DAG deep="<<deep<<" unique="<<nodes.size()<<" entered="<<dag::calls<<std::endl;check(dag::calls<=nodes.size()*4,"The genuine shared linear fixture validates within four entries per unique owning node");}
 auto root=std::make_shared<SourceMaterialBindings>(*holder);root->validate(false);root->freeze_seconds=std::numeric_limits<double>::quiet_NaN();check(refuses([&]{root->validate(false);}),"A later top-level call cannot borrow a previous pointer proof after a real scalar mutation");root->freeze_seconds=holder->freeze_seconds;root->validate(false);check(true,"Exceptional validation clears its scoped ledger and permits exact retry");
 auto recipe=std::make_shared<SourceColorPreparation>(*root->color_preparation);auto uv=std::make_shared<SourceColorUvReprojection>(*recipe->uv_reprojection);uv->before_material=root;recipe->uv_reprojection=uv;auto prior=root->color_preparation;root->color_preparation=recipe;bool cycle=refuses([&]{root->validate(false);});root->color_preparation=prior;uv->before_material.reset();check(cycle,"A shared-node ledger never hides an actual active-path cycle");root->validate(true);check(true,"Cycle failure leaves the exact immutable source graph valid for deep retry");
 std::cout<<"PASS "<<checks<<" bounded material DAG checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
