// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NATIVE_PHYSICAL_CONTRACTS_NO_MAIN
#include "native-physical-contracts.cpp"
#undef DOME_NATIVE_PHYSICAL_CONTRACTS_NO_MAIN
#include "native-paint32-fixture.hpp"
#include "emission-export.hpp"
#include "humanoid-sha256.hpp"
#include <charconv>
namespace {
size_t prefix_number(const char *text){
 const std::string_view value(text);size_t result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
 if(value.empty()||error!=std::errc{}||end!=value.data()+value.size())throw std::invalid_argument("Invalid absolute allocation prefix");return result;
}
void prefix_report(const std::filesystem::path &file,const nlohmann::json &report){
 std::ofstream stream(file);stream.exceptions(std::ios::badbit|std::ios::failbit);stream<<report.dump(2)<<'\n';
}
}

int main(int argc,char **argv)try{
 if(argc<3)return 2;const std::string_view mode=argc>3?argv[3]:"";
 const bool full=mode=="--oom",measure=mode=="--oom-measure",range=mode=="--oom-range";
 if((argc!=3&&!full&&!measure&&!range)||((full||measure)&&argc!=4)||(range&&argc!=7))return 2;
 const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);auto scene=Scene::load(argv[1]);scene.validate();const auto exact=scene.encode();auto captured=std::make_shared<NativeEmissionExport>();captured->scene=std::make_shared<const Scene>(scene.snapshot());captured->scope=NativeEmissionExport::Scope::WholeScene;captured->allow_measured_core_fallback=true;GltfExportOptions options;options.emission=captured;
 auto emit=[&](const std::filesystem::path &path,const GltfExportOptions &settings){auto surfaces=emission_export_surfaces(*captured->scene,settings.cancelled);return export_glb(surfaces,path,settings);};
 check(scene.active().material_bindings->canonical_paint_sampling&&scene.active().painted->has_physical_emission(),"Contracts exercise genuine canonical37 retained float paint rather than Source4");set_paint_composite_cache_budget(0);
 const auto file=out/"direct.glb",baseline_file=out/"baseline.glb";
 if(full||measure||range){
  const auto report_file=out/(measure?"measurement.json":"range.json");
  if(!full&&std::filesystem::exists(report_file))throw std::invalid_argument("Allocation evidence already exists; use a fresh output directory");
  size_t baseline=0;{allocation_probe::Guard guard(SIZE_MAX);emit(baseline_file,options);baseline=allocation_probe::calls;}
  std::cout<<"CANONICAL37_PHYSICAL_BASELINE "<<baseline<<std::endl;
  nlohmann::json evidence;size_t begin=0,end=baseline+1;
  if(!full){
   std::vector<size_t> repeated{baseline};
   for(int repeat=0;repeat<2;++repeat){size_t count=0;{allocation_probe::Guard guard(SIZE_MAX);emit(baseline_file,options);count=allocation_probe::calls;}repeated.push_back(count);}
   const auto recovered=extract_native_emission(bytes(baseline_file));
   if(!recovered||recovered->encode()!=exact||scene.encode()!=exact||captured->scene->encode()!=exact)throw std::runtime_error("Allocation calibration changed complete owner or recovered package");
   evidence={{"format",1},{"mode",measure?"measure":"range"},{"baseline",baseline},{"baseline_counts",repeated},
    {"fixture_sha256",creator_detail::sha256(bytes(argv[1]))},{"scene_sha256",creator_detail::sha256(nlohmann::json::to_cbor(exact))},
    {"baseline_package_sha256",creator_detail::sha256(bytes(baseline_file))},{"cache_budget",0}};
#ifdef __SANITIZE_ADDRESS__
   evidence["profile"]="asan-ubsan";
#else
   evidence["profile"]="release";
#endif
   const bool stable=std::all_of(repeated.begin(),repeated.end(),[&](size_t n){return n==baseline;});evidence["stable_baseline"]=stable;
   if(measure){evidence["terminal_exit"]=stable?0:1;prefix_report(report_file,evidence);if(!stable)throw std::runtime_error("Uninjected allocation baseline is not stable");return 0;}
   if(!stable||prefix_number(argv[6])!=baseline)throw std::runtime_error("Allocation partition baseline differs from its measured frozen plan");
   begin=prefix_number(argv[4]);end=prefix_number(argv[5]);
   if(begin>=end||end>baseline+1)throw std::invalid_argument("Allocation partition is outside the complete absolute prefix interval");
  }
  size_t denied=0,successful=0,translated=0;bool endpoint_successful=false;std::vector<size_t> retried_prefixes;std::set<std::string> messages;const auto started=std::chrono::steady_clock::now();
  for(size_t prefix=begin;prefix<end;++prefix){seed(file);const auto prior=bytes(file);bool failed=false,other=false;std::exception_ptr error;size_t calls=0;
   {allocation_probe::Guard guard(prefix);try{emit(file,options);}catch(const std::bad_alloc&){failed=true;}catch(const std::exception&e){failed=other=true;error=std::current_exception();}calls=allocation_probe::calls;}
   std::string message;if(error)try{std::rethrow_exception(error);}catch(const std::exception&e){message=e.what();}
   if(other&&calls<=prefix)throw std::runtime_error(std::string("Unrelated export error in allocation sweep: ")+message);
   if(failed){if(bytes(file)!=prior||scene.encode()!=exact)throw std::runtime_error("Canonical37 physical allocation denial changed prior output or complete source");++denied;if(other){++translated;if(messages.insert(message).second)std::cout<<"CANONICAL37_TRANSLATED_DENIAL "<<message<<std::endl;}}
   else {auto restored=extract_native_emission(bytes(file));if(!restored||restored->encode()!=exact)throw std::runtime_error("Successful allocation prefix failed exact canonical37 recovery");++successful;if(prefix==baseline)endpoint_successful=true;}
   if(prefix==0||prefix==baseline/2||prefix==baseline){emit(out/"retry.glb",options);auto retry=extract_native_emission(bytes(out/"retry.glb"));check(retry&&retry->encode()==exact,"Early/middle/final persistent denial contexts retry exact complete project");retried_prefixes.push_back(prefix);}
   if(prefix%250==0)std::cout<<"CANONICAL37_PHYSICAL_PREFIX "<<prefix<<std::endl;
  }
  check((full?(denied&&successful):(denied+successful==end-begin))&&scene.encode()==exact,
   "All assigned calling-thread C++ prefixes retain exact file/source or complete successful package");
  if(!full){
   if(captured->scene->encode()!=exact||creator_detail::sha256(bytes(argv[1]))!=evidence.at("fixture_sha256").get_ref<const std::string&>())throw std::runtime_error("Allocation partition changed captured owner or input fixture");
   evidence.update({{"begin",begin},{"end",end},{"completed",denied+successful},{"denied",denied},{"successful",successful},{"translated",translated},
    {"endpoint_successful",endpoint_successful},{"retried_prefixes",retried_prefixes},{"first_completed",begin},{"last_completed",end-1},
    {"seconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()},{"terminal_exit",0}});prefix_report(report_file,evidence);
  }
std::cout<<"CANONICAL37_PHYSICAL_PREFIXES denied="<<denied<<" successful="<<successful<<" baseline="<<baseline<<" translated="<<translated<<std::endl;return 0;
 }
 emit(file,options);const auto original=bytes(file);auto native=extract_native_emission(original);check(native&&native->encode()==exact,"Real physical package4 entrypoint recovers complete exact canonical37 source");
 const auto field=detail::canonical_paint_emission_field(scene.active(),{});const auto encoded=encode_emission_exr(field);EmissionInterchangeLimits codec;codec.max_encoded_bytes=encoded.size();check(encode_emission_exr(field,codec)==encoded,"Exact compressed EXR byte cap preserves encoded physical data");--codec.max_encoded_bytes;check(refusal([&]{encode_emission_exr(field,codec);},"budget"),"One below actual compressed EXR size refuses through bounded stream without publication");
 auto bounded=options;bounded.max_output_bytes=original.size();emit(file,bounded);check(bytes(file)==original,"Exact complete physical container limit preserves successful bytes");--bounded.max_output_bytes;check(refusal([&]{emit(file,bounded);},"budget")&&bytes(file)==original,"One byte below complete physical container limit refuses without publication");
 NativeEmissionReadLimits limits;limits.max_container_bytes=original.size();check(extract_native_emission(original,limits)->encode()==exact,"Exact physical reader container limit remains valid");--limits.max_container_bytes;check(refusal([&]{extract_native_emission(original,limits);},"limits"),"One below physical reader container limit refuses before decode");
 auto word=[](const auto &b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;};const auto json_size=word(original,12);auto json=nlohmann::json::parse(original.begin()+20,original.begin()+20+json_size);const auto &entry=json.at("extras").at("domeSculptEmission").at("native");const auto stored=json.at("bufferViews").at(entry.at("bufferView").get<size_t>()).at("byteLength").get<size_t>();limits={};limits.max_native_bytes=stored;check(extract_native_emission(original,limits)->encode()==exact,"Exact stored canonical37 owner byte cap recovers unchanged source");--limits.max_native_bytes;check(refusal([&]{extract_native_emission(original,limits);},"payload exceeds read budget"),"One below actual stored owner cap refuses without reconstruction");
 limits={};limits.image.max_pixels=size_t(scene.active().painted->resolution())*scene.active().painted->resolution();limits.image.max_dimension=scene.active().painted->resolution();check(extract_native_emission(original,limits)->encode()==exact,"Exact FLOAT32 native field dimensions remain representable");--limits.image.max_pixels;check(refusal([&]{extract_native_emission(original,limits);},"limit"),"One below actual FLOAT32 field pixel cap refuses");
 size_t polls=0;auto counted=options;counted.cancelled=[&]{++polls;return false;};emit(file,counted);for(size_t at:{size_t(1),polls/4,polls/2,polls*3/4,polls-1}){size_t calls=0;auto cancel=counted;cancel.cancelled=[&]{return ++calls>=at;};check(refusal([&]{emit(file,cancel);},"cancelled")&&bytes(file)==original&&scene.encode()==exact,"Physical capture/field/codec/write cancellation leaves exact old file and source");}
 size_t reads=0;limits={};limits.cancelled=[&]{++reads;return false;};extract_native_emission(original,limits);for(size_t at:{size_t(1),reads/2,reads-1}){size_t calls=0;limits.cancelled=[&]{return ++calls>=at;};check(refusal([&]{extract_native_emission(original,limits);},"cancelled")&&scene.encode()==exact,"Owning package4 parse/replay cancellation publishes no partial project");}
 Document doc;doc.replace(scene.snapshot());doc.edit_scene("Name package4 transaction",[](Scene &s,const auto &){s.active().name="Canonical37 physical queued owner";});settled(doc);const auto edited=doc.copy_scene().encode();const auto state=doc.frame().state;const auto running=out/"running.glb",queued=out/"queued.glb",saved=out/"ordered.dsculpt";seed(running);seed(queued);const auto prior=bytes(running);
 {durable_gate::Guard guard;durable_gate::arm();auto a=doc.export_asset(running,AssetExportFormat::Glb);check(durable_gate::wait(),"Actual package4 file worker reaches durable temporary output before commit");auto b=doc.export_asset(queued,AssetExportFormat::Glb);auto save=doc.save_async(saved);check(doc.frame().state->pending_exports==2&&doc.frame().state->can_cancel_asset,"Running and queued package4 work share actual Cancel ownership");doc.cancel_background_work();durable_gate::release();check(cancelled(std::move(a))&&cancelled(std::move(b)),"Actual running and queued physical exports both cancel before publication");save.get();check(bytes(running)==prior&&bytes(queued)==prior&&Scene::load(saved).encode()==edited&&doc.copy_scene().encode()==edited&&doc.frame().state->can_undo==state->can_undo&&doc.frame().state->can_redo==state->can_redo,"Physical Cancel preserves both files, complete live project/history and following Save");}
 doc.export_asset(running,AssetExportFormat::Glb).get();auto retried=extract_native_emission(bytes(running));check(retried&&retried->encode()==edited&&!doc.frame().state->pending_exports,"Real canonical37 physical export retries successfully after queued Cancel");doc.undo();check(doc.copy_scene().encode()==exact,"Following physical export preserves preexisting Undo endpoint");doc.redo();check(doc.copy_scene().encode()==edited,"Following physical export preserves preexisting Redo endpoint");check(scene.encode()==exact,"Every physical boundary/worker test leaves original source untouched");std::cout<<"PASS "<<checks<<" canonical37 physical contracts\n";return 0;
}catch(const std::exception &e){durable_gate::release();std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
