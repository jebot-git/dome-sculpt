// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-weight-retention.hpp"
#include "dome/mtoon.hpp"
#include <iostream>
using namespace dome;using namespace dome::detail;
namespace {size_t checks=0;void check(bool x,const char*s){if(!x)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<'\n';}
Mesh plane(){Mesh m;m.positions={{0,0,0},{1,0,0},{0,1,0}};m.normals.assign(3,{0,0,1});m.uv={{{0,0}},{{1,0}},{{0,1}}};m.indices={0,1,2};return m;}}
int main()try{
 using native_weight_retention_impl::Counter;
 auto holder=std::make_shared<std::pair<std::vector<uint8_t>,std::vector<uint8_t>>>();holder->first.resize(4096);holder->second.resize(8192);
 std::shared_ptr<const std::vector<uint8_t>>a(holder,&holder->first),b(holder,&holder->second);
 Counter c(2u*1024u*1024u);c.shared_vector(a);auto one=c.used;c.shared_vector(a);check(c.used==one,"Repeated exact object/control pair is charged once");c.shared_vector(b);check(c.used>=one+8192,"Different alias object under the same control retains its separate buffer");
 std::shared_ptr<const std::vector<uint8_t>>foreign(&holder->second,[](const auto*){});auto prior=c.used;c.shared_vector(foreign);check(c.used>=prior+8192,"Same address with foreign control block cannot reuse a retention proof");
 std::shared_ptr<const void>opaque(holder,&holder->first);prior=c.used;if(c.enter(opaque)){c.add(128);c.leave(opaque);}check(c.used>prior,"Opaque/type-distinct alias cannot masquerade as another complete object proof");
 auto image=std::make_shared<Image>(Image{64,64,std::vector<uint8_t>(64*64*4)});Counter im(2u*1024u*1024u);im.image(image);auto bytes=im.used;im.image(image);check(im.used==bytes,"Shared image owner deduplicated without reading or copying its plane");
 auto binding=std::make_shared<SourceMaterialBindings>();binding->material=MToonMaterial::from_vrm1({{"x-budget-proof",std::string(262144,'x')},{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});Counter material(2u*1024u*1024u);auto base=material.used;material.material(binding);check(material.used>=base+262144,"Actual retained private/unknown MToon JSON payload is included");
 auto x=std::make_shared<PolygonOrigin>(),y=std::make_shared<PolygonOrigin>();x->rig_source=y;y->shape_source=x;bool cycle=false;try{Counter loop(2u*1024u*1024u);loop.origin(x);}catch(const std::invalid_argument&e){cycle=std::string(e.what()).find("active cycle")!=std::string::npos;}y->shape_source.reset();x->rig_source.reset();check(cycle,"Typed cross-origin active cycles refuse instead of being mistaken for deduplication");
 auto d=std::make_shared<DeformationState>();d->skeleton.emplace();d->skeleton->bones.resize(257);bool dimensions=false;try{Counter bad(2u*1024u*1024u);bad.deformation(d);}catch(const std::length_error&e){dimensions=std::string(e.what()).find("dimensions")!=std::string::npos;}check(dimensions,"Illegal control dimensions refuse before walking/copying their values");
 std::vector<std::shared_ptr<const std::vector<uint8_t>>>owners;owners.reserve(4097);for(size_t i=0;i<4097;++i)owners.push_back(std::make_shared<const std::vector<uint8_t>>());bool quota=false;try{Counter limit(2u*1024u*1024u);for(auto&p:owners)limit.shared_vector(p);}catch(const std::length_error&e){quota=std::string(e.what()).find("node cap")!=std::string::npos;}check(quota,"Actual unique-owner ledger cap is bounded");
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image;auto paint=PaintedMesh::from_maps(plane(),maps,64);auto copy=NativeWeightPaintAccess::snapshot(*paint);Material mat;check(copy->preview(mat)!=nullptr,"Detached current paint keeps a valid empty cache for Restore/preview");
 Counter raster(2u*1024u*1024u);raster.paint(copy);auto before=raster.used;auto second=NativeWeightPaintAccess::snapshot(*copy);raster.paint(second);check(raster.used>before&&raster.used-before<65536,"Fresh paint wrapper is charged while exact immutable full SurfaceMap owner is shared once");
 std::cout<<"Native anatomical retention controls "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
