// SPDX-License-Identifier: GPL-3.0-or-later
#define main detached_kernel_fault_fixture
#include "canonical-normal-conversion-faults.cpp"
#undef main
#include "canonical-normal-owner.hpp"

int main()try{
 auto scene=baked_fan(true,true,true);const auto exact=scene.encode();
 const auto paint=scene.active().painted;const auto deformation=scene.active().deformation;
 const ColorPreparationInput input{paint,{},nullptr,deformation,scene.active().material,{}};
 auto produce=[&]{return convert_canonical_normals(input);};
 auto signature=[](const NormalPreparationResult &r){
  NormalFrameArchive frames;frames.add(r.material_bindings->normal_frames);
  const auto &p=*r.material_bindings->normal_preparation;
  return nlohmann::json{{"paint",r.paint->encode()},{"frames",frames.encode()},
   {"receipt",p.canonical_conversion->encode()},{"before",p.original_paint->encode()}};
 };
 auto warm=produce();const auto expected=signature(warm);
 conversion_fault::calls=0;conversion_fault::at=SIZE_MAX;conversion_fault::enabled=true;
 auto positive=produce();conversion_fault::enabled=false;const auto count=conversion_fault::calls;
 require_cage(count>69&&count<16384&&signature(positive)==expected,
   "Complete public owning conversion allocation prefixes include snapshot, lease, paint adoption and binding validation");
 for(size_t prefix=1;prefix<=count;++prefix){
  conversion_fault::calls=0;conversion_fault::at=prefix;conversion_fault::hit=false;
  bool refused=false;conversion_fault::enabled=true;
  try{produce();}catch(const std::bad_alloc&){refused=true;}
  conversion_fault::enabled=false;
  if(!refused||!conversion_fault::hit||scene.encode()!=exact||scene.active().painted!=paint||scene.active().deformation!=deformation)
   throw std::runtime_error("Owning conversion allocation failure changed input or missed its actual measured boundary");
  if(signature(produce())!=expected)throw std::runtime_error("Owning conversion allocation retry changed genuine source/output data");
 }
 size_t polls=0;NormalPreparationOptions options;options.cancelled=[&]{++polls;return false;};convert_canonical_normals(input,options);
 for(size_t cut=1;cut<=polls;++cut){size_t at=0;options.cancelled=[&]{return ++at>=cut;};
  if(!failure([&]{convert_canonical_normals(input,options);},"cancel")||scene.encode()!=exact)
   throw std::runtime_error("Owning conversion cancellation failed exact input preservation");
 }
 options={};options.max_working_bytes=64*1024;
 require_cage(failure([&]{convert_canonical_normals(input,options);},"memory budget")&&scene.encode()==exact,
   "Prospective snapshot/lease memory denial precedes owning conversion publication");
 options={};options.max_samples=1;
 require_cage(failure([&]{convert_canonical_normals(input,options);},"sample budget")&&scene.encode()==exact&&signature(produce())==expected,
   "Public owning sample denial and all cancellation endpoints retry to the exact complete original/output state");
 std::cout<<"PASS owning_persistent_cpp_prefixes="<<count<<" all_cancel_polls="<<polls<<'\n';return 0;
}catch(const std::exception &e){conversion_fault::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
