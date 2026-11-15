"""Read-only genuine App1bc component LINK plan, no configure/stage/link/app/ldd."""
import hashlib,json,os,re,shlex,subprocess,sys,time
from pathlib import Path
P=Path(__file__).resolve().parent;ROOT=P.parents[1];HELPER=ROOT/'artifacts/native90-release-planning-20261115';sys.path.append(str(HELPER))
from provider_context_v2 import seal_context
from component_link_common import sha as uncached_sha,compiler_dependencies,ld_dependencies,dynamic_metadata,codegen_context
from ninja_audit import archive_members
pins={};cache={};began=time.monotonic()
def sha(path):
 p=Path(path);s=p.stat();key=(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns);old=cache.get(str(p))
 if old and old[0]==key:return old[1]
 h=uncached_sha(p);after=p.stat();assert key==(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns);cache[str(p)]=(key,h);return h
def pin(path,expected=None):
 p=Path(path);h=sha(p)
 if expected is not None:assert h==expected,('Pinned expectation differs',str(p),expected,h)
 assert str(p)not in pins or pins[str(p)]==h,('Conflicting expected provider',str(p));pins[str(p)]=h;return h
def load(path,expected=None):pin(path,expected);return json.loads(Path(path).read_bytes())
def gitcopy(commit,relative,name,expected=None):
 raw=subprocess.check_output(['git','show',commit+':'+relative],cwd=ROOT);p=P/name;assert not p.exists();p.write_bytes(raw);pin(p,expected);return json.loads(raw)
ctx=seal_context(ROOT,pin,sha);old=ctx['plan'];OLD=ctx['old_source'];BUILD=ctx['old_build'];TRIAL=P/'trial-v1';assert not TRIAL.exists()
source=ROOT/'artifacts/artist-prepared-source-v3-20261115';head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip();assert head.startswith('1bc13125')and not subprocess.check_output(['git','status','--porcelain'],cwd=source,text=True).strip()
tracked=subprocess.check_output(['git','ls-files','-z'],cwd=source,text=True).split('\0');current={x:pin(source/x)for x in tracked if x};prior=ctx['tree']
product=lambda k:k.startswith(('src/','include/','third_party/','assets/'))or Path(k).suffix in{'.glsl','.vert','.frag','.comp'}
changed=sorted(k for k in set(prior)|set(current)if product(k)and prior.get(k)!=current.get(k));expected_delta=['src/bake-authoring.cpp','src/bake.cpp','src/emission-bake-source.cpp','src/source-bake-normal.hpp'];assert changed==expected_delta,(changed,expected_delta)
shared_headers=[k for k in prior if product(k)and Path(k).suffix in{'.hpp','.h','.hh','.hxx','.inc'}];assert all(current[k]==prior[k]for k in shared_headers)
native=gitcopy('ea13136d','docs/validation/ARTIST-PREPARED-SOURCE-NATIVE-20261115.json','native-qualification.json')
graphics=gitcopy('1b4fb828','docs/validation/ARTIST-NORMAL-GRAPHICS-20261115.json','graphics-qualification.json','c2b02faf88b4e31a379ee55fa094af6f6d6ebd80060b7227bf6efb5eda5a287a')
assert native['product_source']==graphics['functional_product']=='1bc13125'and graphics['state']=='qualified-component-milestone'
assert native['public_headers_shaders_packets_native_wires_unchanged']and graphics['product_footprint']['all_src_include_third_party_bytes_exact_product1bc']
for key,h in graphics['product_footprint']['sha256'].items():assert current[key]==h
for profile,row in graphics['graphical_profiles'].items():
 s=load(row['status_path'],row['status_sha256']);assert s['state']=='terminal'and s['exit']==0 and not s['input_drift'];pin(row['summary_path'],row['summary_sha256'])
component=ROOT/'artifacts/artist-prepared-release-component-v3-20261115';status=load(component/'status.json','8ce2cb759742045cf4ba837e3b671d5a2d28a8dedbdb51b135c3f4699f26e8fb');assert status['exit']==1 and not status['input_drift']
assert all(next(x for x in status['steps']if x['name']==name)['exit']==0 for name in['archive-fixed','index-fixed'])
creation_inputs=load(component/'inputs-before-runtime.json','c63b6989d150ef7201e0cab7deee425880758a7d111d26629bb2b1fbb5a7e4a4')
for path,h in creation_inputs.items():pin(path,h)
core=component/'fixed-core.a';pin(core,'378b5560c1be841225fe02b1b48e4766f3db463aff93c3ef6bb5401daaead890')
replacements={
 'bake-authoring.cpp.o':('artist-prepared-proof-v8-20261115',2),
 'bake.cpp.o':('artist-prepared-proof-v9-20261115',1),
 'emission-bake-source.cpp.o':('artist-prepared-proof-v9-20261115',0)}
actual_compile={}
for basename,(folder,index)in replacements.items():
 p=ROOT/'artifacts'/folder;cplan=load(p/'compile-plan.json');cstatus=load(p/'compile-status.json');cpins=load(p/'compile-inputs.json');assert cstatus['state']=='terminal'and cstatus['exit']==0 and not cstatus['input_drift']
 for path,h in cpins.items():pin(path,h)
 row=cplan['commands'][index];command=row['command'];assert '-MMD'in command and '-MD'not in command
 assert next(x for x in cstatus['steps']if x['name']=='compile-'+str(index))['command']==command
 assert all(next(x for x in cstatus['steps']if x['name']==name)['exit']==0 for name in['compile-'+str(index),'preprocess-'+str(index)])
 compiled_source=Path(cplan['source']);relative=row['file'];assert pin(compiled_source/relative)==current[relative]
 genuine=compiler_dependencies(p/(basename[:-2]+'.d'));full=compiler_dependencies(p/(str(index)+'.preprocess.d'))
 for name in genuine['absolute_names']+full['absolute_names']:
  q=Path(name);physical=str(q.resolve());assert physical in cpins and pin(q)==cpins[physical]
  if q.is_relative_to(compiled_source):assert current[str(q.relative_to(compiled_source))]==cpins[physical]
 original_key=next(k for k in old['links']['libdome-sculpt-core.a']['object_inputs']if Path(k).name==basename);original_command=old['objects'][original_key]['command']
 assert codegen_context(original_command,OLD)==codegen_context(command,compiled_source),('Actual codegen/literal-D context differs',basename)
 object_path=p/basename;pin(object_path)
 actual_compile[basename]={'object':str(object_path),'sha256':sha(object_path),'actual_command':command,'source_root':str(compiled_source),'source_sha256':current[relative],'compile_status':str(p/'compile-status.json'),'compile_status_sha256':sha(p/'compile-status.json'),'actual_user_MMD':genuine,'separate_complete_M_preprocess':full,'original_inherited_CWD':'Not recorded by original driver; not fabricated. Every source/include/output/dep argument and actual dependency name is absolute.','not_called_full_actual_MD':True,'codegen_and_all_literal_D_match_old3d_context':True}
link=old['links']['dome-sculpt'];assert len(link['object_inputs'])==27
archive_keys=list(dict.fromkeys(link['dependencies']));assert len(archive_keys)==14
objects={k:{'provider':str(BUILD/k),'sha256':ctx['owned'][k],'actual_command':old['objects'][k]['command'],'actual_compile_cwd':str(BUILD),'actual_dependency_record':ctx['actual_deps']['records'][k]}for k in link['object_inputs']}
archives={};owner_count=0;core190=0;other_count=0
for key in archive_keys:
 members=archive_members(core if key=='libdome-sculpt-core.a'else BUILD/key);keys=old['links'][key]['object_inputs'];assert len(members)==len(keys)
 expected=[]
 for member,owner in zip(members,keys):
  assert member['name']==Path(owner).name
  if key=='libdome-sculpt-core.a'and member['name']in actual_compile:
   proof=actual_compile[member['name']];assert member['sha256']==proof['sha256']and member['bytes']==Path(proof['object']).stat().st_size
  else:
   assert member['sha256']==ctx['owned'][owner]and member['bytes']==(BUILD/owner).stat().st_size
   if key=='libdome-sculpt-core.a':core190+=1
   else:other_count+=1
  expected.append({'owner_key':owner,**member})
 provider=core if key=='libdome-sculpt-core.a'else BUILD/key;pin(provider)
 archives[key]={'provider':str(provider),'sha256':sha(provider),'members':members,'ordered_owner_proof':expected};owner_count+=len(members)
assert len(archives['libdome-sculpt-core.a']['members'])==193 and core190==190 and other_count==22 and owner_count==215
# This is a new current forecast derived from the genuine original full link record.
# Additional startup objects not previously pinned are explicitly observed now, not given invented historical hashes.
old_record=BUILD/'CMakeFiles/dome-sculpt.dir/link.d';pin(old_record);ld=ld_dependencies(old_record);assert ld['target']=='dome-sculpt'and len(ld['names'])==174
physical={};roster=[]
for name in ld['names']:
 q=Path(name)
 if not q.is_absolute():
  assert name in objects or name in archives,('Unstaged relative link dependency',name)
  row=objects.get(name)or archives[name];digest=row['sha256'];target=TRIAL/name;origin=row['provider'];authority='presealed copied whole input'
 else:target=q;origin=str(q);digest=pin(q);authority='current external provider observed before new link from genuine old link roster; historical identity claimed only where prior pins already existed'
 physical[str(target.resolve())]=digest;roster.append({'literal':name,'future_path':str(target),'sha256':digest,'provider':origin,'authority':authority})
env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
for key in['LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']:env.pop(key,None)
ldpath=str(ROOT/'artifacts/sdk-20261111/toolkit-release/lib')+':/opt/fltk-vr/bdf-runtime/lib:/usr/lib64';env['LD_LIBRARY_PATH']=ldpath
pin('/usr/bin/readelf');pin(Path('/usr/bin/readelf').resolve());pin('/bin/bash');pin(Path('/bin/bash').resolve())
metadata=dynamic_metadata(BUILD/'dome-sculpt',env)
preflight=load(ROOT/'artifacts/guard87-validation-fresh-v2-20261115/runtime-preflight.json','949716280d10503f4899694790c1ade8779c7acce793b6f6c6d78408529129d4')
for name in['prepare-link-v1.py','produce-link-v1.py','component_link_common.py','ninja_audit.py','owned_monitor_v2.py']:pin(P/name)
for name in['provider_context_v2.py','current_provider_routes_v2.py']:pin(HELPER/name)
assert not TRIAL.exists()and all(sha(path)==h for path,h in pins.items())
plan={'state':'prepared-NOT-linked','scope':'Additive App1bc application-component trial for public-UI/XR test development. Not whole-owning0ae/native90, main/runtime/install admission or replacement of planned whole90. All sources/frozen providers untouched; no project binary/ldd invoked in preparation.','scope_tuple':{'application_product':'1bc13125','unchanged_component_context':'3d164382','literal_asset_context':'3d164382'},'functional_source':{'path':str(source),'head':head,'changed_product_paths_vs3d':changed,'shared_existing_ABI_header_count':len(shared_headers),'shared_existing_headers_shaders_packets_native_wires_assets_byte_match':True,'scope':'Exact source/body/interface hashes, not fabricated binary-layout or runtime ABI test'},'authority':ctx['authority'],'component_native_qualification':{'commit':'ea13136d','receipt_path':str(P/'native-qualification.json'),'sha256':sha(P/'native-qualification.json')},'component_graphics_qualification':{'commit':'1b4fb828','receipt_path':str(P/'graphics-qualification.json'),'sha256':sha(P/'graphics-qualification.json')},'original_core_creation':{'overall_exit':1,'archive_fixed_exit':0,'index_fixed_exit':0,'qualified_same_exact_core_later':True,'status_sha256':sha(component/'status.json')},'summary':{'whole_app_UI_objects':27,'ordered_unique_archives':14,'core_members':193,'core_unchanged_members':190,'genuine_replacement_compile_owners':3,'other_archive_owners':22,'total_complete_owner_members':242,'fresh_complete_app_links':1,'old_executables_reused':0,'new_compilations':0},'objects':objects,'archives':archives,'three_actual_compilers':actual_compile,'destination':str(TRIAL),'original_actual_link_command':link['command'],'command':link['command'],'command_policy':'VERBATIM actual old3d app linker command; only CWD is the new isolated trial with exact whole input copies. No -D/path/flag rewrite.','link_dependency_forecast':{'source_record':str(old_record),'source_record_sha256':sha(old_record),'raw_actual_original':ld,'literal_names':ld['names'],'expected_physical':physical,'roles':roster,'future_actual_record_required':True,'not_current_link_proof':True},'old_application_dynamic_metadata':metadata,'loader_forecast':{'prior_preflight_path':str(ROOT/'artifacts/guard87-validation-fresh-v2-20261115/runtime-preflight.json'),'prior_preflight_sha256':'949716280d10503f4899694790c1ade8779c7acce793b6f6c6d78408529129d4','new_app_hash':None,'new_app_state':'must come from actual fresh producer','no_new_loader_admission':True,'old_app_pin_not_overwritten':True,'future_NEEDED_RUNPATH_RPATH_must_match_old':True,'actual_new_product_ldd_only_after_common_lease_coordination':True},'environment':{'LD_LIBRARY_PATH':ldpath},'limits':{'one_link_seconds':900,'aggregate_child_RSS_bytes':4*2**30,'virtual_AS_cap':None},'pins':pins,'prepare_initial_final_drift':[],'producer_execution_requires_root_review':True,'seconds':time.monotonic()-began}
(P/'plan-v1.json').write_text(json.dumps(plan,indent=2)+'\n');print(json.dumps({'state':plan['state'],'summary':plan['summary'],'plan_sha256':sha(P/'plan-v1.json'),'pins':len(pins),'seconds':plan['seconds']},indent=2))
