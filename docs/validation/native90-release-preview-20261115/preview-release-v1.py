"""Read-only successor impact preview using an EXISTING root-reviewed graph.
No configure, object staging, compilation, link, product or graphics invocation.
This preview is never an executable owning build plan or final source freeze.
"""
import collections,gzip,hashlib,json,os,pathlib,re,shlex,subprocess,time
from current_provider_routes import decide_current_whole_object,new_header_basenames
P=pathlib.Path(__file__).resolve().parent;R=P.parents[1]
BASE=R/'artifacts/current-toolchain-guard87-release-20261115';PREVIEW=R/'artifacts/normal-integration-configure-review-20261115';NEW_SOURCE=R/'artifacts/authoring-normal-integration-20261115';NEW_BUILD=PREVIEW/'release'
OUT=P/'preview-0ae03214-v1';OUT.mkdir()
HEAD='0ae03214';PROOF_EXPECTED='39c71b267c63127b68b40acc959df021fd662a56fad38c809d0a92af741c6f3e';PLAN_EXPECTED='0d03a29a1d2251d1186cb8a9c94f22227566c86664245c51b050a66e458b7dec'
cache={};pins={};started=time.monotonic()
def sha(p):
 p=pathlib.Path(p);a=p.stat();ident=(a.st_dev,a.st_ino,a.st_size,a.st_mtime_ns,a.st_ctime_ns);key=str(p)
 if key in cache and cache[key][0]==ident:return cache[key][1]
 with p.open('rb')as f:h=hashlib.file_digest(f,'sha256').hexdigest()
 b=p.stat();assert ident==(b.st_dev,b.st_ino,b.st_size,b.st_mtime_ns,b.st_ctime_ns),('Input changed during read',key)
 cache[key]=(ident,h);return h
def pin(p,expected=None):
 p=pathlib.Path(p);h=sha(p)
 if expected is not None:assert h==expected,('Sealed expectation differs',str(p),expected,h)
 assert str(p)not in pins or pins[str(p)]==h;pins[str(p)]=h;return h
def load(p,expected=None):pin(p,expected);return json.loads(pathlib.Path(p).read_bytes())
def git(*args):return subprocess.check_output(['git',*args],cwd=NEW_SOURCE,text=True).strip()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
proof=load(BASE/'closed-artifacts-independent-audit-v3.json',PROOF_EXPECTED);assert proof['state']=='passed'and proof['exit']==0
plan_path=BASE/'release-3d1643-current-fresh-v2-audit/plan.json';old=load(plan_path,PLAN_EXPECTED);OLD_SOURCE=pathlib.Path(old['source_path']);OLD_BUILD=pathlib.Path(old['destination'])
status=load(plan_path.parent/'execution-v1/status.json');assert status==proof['driver_sealed_result']and status['exit']==0 and not status['input_drift']
assert proof['outer_tool_result']=={'session':12734,'exit':143,'cause':'unknown; not inferred from successful driver sealing'}
oldpins=load(status['inputs']['path'],status['inputs']['sha256']);assert len(oldpins)==30961
for p,h in oldpins.items():pin(p,h)
owned=load(status['owned_outputs']['path'],status['owned_outputs']['sha256']);assert len(owned)==1768
for k,h in owned.items():pin(OLD_BUILD/k,h)
deps=load(status['actual_fresh_compiler_dependencies']['path'],status['actual_fresh_compiler_dependencies']['sha256']);assert deps['owners']==1032 and not deps['errors'] and len(deps['records'])==1032
# Bind the actual raw -MD/-MF text to every sealed owner record, not a fabricated current .d.
for key,row in deps['records'].items():
 f=pathlib.Path(row['depfile']);pin(f,row['depfile_sha256']);raw=f.read_text().replace('\\\n',' ');target,body=raw.split(':',1)
 assert shlex.split(target)==row['actual_target']
 actual=[str((pathlib.Path(x)if pathlib.Path(x).is_absolute()else OLD_BUILD/x).resolve())for x in shlex.split(body)]
 assert actual==[x['resolved']for x in row['dependencies']],('Raw actual dependency list differs',key)
 for entry in row['dependencies']:pin(entry['path'],entry['expected_sha256'])
# Tools and their current closure must remain exact to the admitted current toolchain.
for p,row in old['current_toolchain']['tools'].items():pin(p,row['sha256']);pin(pathlib.Path(row['resolved']),row['sha256'])
for p,h in old['current_toolchain']['ELF_forecast']['pins'].items():pin(p,h)
review=load(PREVIEW/'status.json');assert review['exit']==0 and review['configured_TUs']==1036 and review['compiled_objects']==0
for p,h in review['configured_file_hashes'].items():pin(p,h)
head=git('rev-parse','HEAD');assert head.startswith(HEAD)and not git('status','--porcelain')
tracked=git('ls-files','-z').split('\0');newtree={x:pin(NEW_SOURCE/x)for x in tracked if x}
oldtree={str(pathlib.Path(p).relative_to(OLD_SOURCE)):h for p,h in old['source_tree'].items()}
# Existing graph was configured at1d2; bind graph compatibility explicitly without claiming a new configure.
config_names=[x for x in tracked if x and (pathlib.Path(x).name=='CMakeLists.txt'or pathlib.Path(x).suffix=='.cmake')]
config_equal=[]
for name in config_names:
 prior=subprocess.check_output(['git','show',review['source']+':'+name],cwd=NEW_SOURCE)
 assert hashlib.sha256(prior).hexdigest()==newtree[name],('Existing graph is stale for current CMake input',name)
 config_equal.append(name)
assert NEW_SOURCE==pathlib.Path(review['command'][review['command'].index('-S')+1])
commands=json.loads((NEW_BUILD/'compile_commands.json').read_text());keys=[str(pathlib.Path(x['output']).relative_to(NEW_BUILD))for x in commands];assert len(keys)==len(set(keys))==1036
actual=subprocess.check_output(['ninja','-C',str(NEW_BUILD),'-t','commands','-s',*keys],text=True).splitlines();assert len(actual)==1036
assetsold={k:h for k,h in oldtree.items()if k.startswith('assets/')};assetsnew={k:h for k,h in newtree.items()if k.startswith('assets/')};assets_equal=assetsold==assetsnew
sensitive={'third_party/desktop/bdf_font.hpp','src/vroid-creator.cpp','tests/layered-normal-native.cpp','tests/source-rest-ui-fixture.cpp','tests/avatar.cpp','third_party/xatlas/xatlas.cpp','third_party/ufbx/ufbx.c'}
# Broad path/time-token scan must be represented conservatively in the explicit policy.
for name in set(oldtree)|set(newtree):
 if pathlib.Path(name).suffix not in{'.h','.hpp','.hh','.hxx','.inc','.cpp','.cc','.c'}:continue
 for root,tree in [(OLD_SOURCE,oldtree),(NEW_SOURCE,newtree)]:
  if name not in tree:continue
  data=(root/name).read_bytes()
  if any(x in data for x in[b'__FILE__',b'__BASE_FILE__',b'__builtin_FILE',b'source_location',b'__TIMESTAMP__',b'__TIME__',b'__DATE__']):sensitive.add(name)
# Every generator alias is established by the old actual owning plan, not its basename.
generated_aliases={str((OLD_BUILD/key).resolve()):{'key':key,'sha256':row['expected_sha256']}for key,row in old['generated_outputs'].items()}
current_generated_expected={}
for key,row in old['generated_outputs'].items():
 for entry in row['inputs']:
  q=pathlib.Path(entry['current'])
  if q.is_relative_to(OLD_SOURCE):assert newtree[str(q.relative_to(OLD_SOURCE))]==entry['sha256']
  else:pin(q,entry['sha256'])
 current_generated_expected[key]=row['expected_sha256']
rows={};reasons=collections.Counter();extra_sensitive=[]
for key,entry,line in zip(keys,commands,actual):
 argv=shlex.split(line);assert '-D'not in argv and '-MD'in argv and '-MF'in argv and '-MT'in argv
 prior=old['objects'].get(key); record=deps['records'].get(key)
 row=decide_current_whole_object(key=key,new_command=argv,provider_command=prior['command']if prior else None,actual_dependency_record=record,old_source=OLD_SOURCE,new_source=NEW_SOURCE,old_build=OLD_BUILD,new_build=NEW_BUILD,new_tree=newtree,provider_path=OLD_BUILD/key if key in owned else None,provider_sha256=owned.get(key),current_assets_identical=assets_equal,generated_aliases=generated_aliases,current_generated_expected=current_generated_expected,new_headers=new_header_basenames(oldtree,newtree),source_sensitive_paths=sensitive,hash_file=sha)
 rows[key]=row
 for reason in row['reasons']:reasons[reason]+=1
 if any('source-path dependency' in x for x in row['reasons']):extra_sensitive.append(key)
fresh=[k for k,v in rows.items()if v['route']=='compile-complete-TU'];reuse=[k for k in rows if k not in fresh]
# No project output was staged, built or linked; CMake's own configure ABI artifacts are outside this claim.
assert not list(NEW_BUILD.rglob('*.o'))
assert git('rev-parse','HEAD')==head and not git('status','--porcelain')
assert all(sha(p)==h for p,h in pins.items())
raw=json.dumps(rows,indent=2).encode()+b'\n';compressed=gzip.compress(raw,mtime=0);routefile=OUT/'object-route-preview.json.gz';routefile.write_bytes(compressed)
report={'state':'preview-only-NOT-executable-NOT-frozen','exit':0,'current_source':head,'configured_graph_source':review['source'],'existing_configure_record':{'path':str(PREVIEW/'status.json'),'sha256':sha(PREVIEW/'status.json')},'scope':'Read-only impact forecast over existing compatible1d2 graph and current0ae bodies. Not a configure/build invocation, final frozen plan, staged object, link or runtime acceptance. Recompute under final clean source freeze and owning destination.','authority':{'admission_path':str(BASE/'closed-artifacts-independent-audit-v3.json'),'admission_sha256':PROOF_EXPECTED,'baseline_plan_sha256':PLAN_EXPECTED,'original_outer143_preserved':True,'actual_dependency_manifest_sha256':status['actual_fresh_compiler_dependencies']['sha256'],'actual_owners':1032,'actual_raw_depfiles_rechecked':1032,'all_old_input_pins_rechecked':len(oldpins),'all_old_owned_outputs_rechecked':len(owned),'current_toolchain_paths':len(old['current_toolchain']['tools'])},'summary':{'configured_TUs':1036,'candidate_fresh_complete_TUs':len(fresh),'candidate_reusable_complete_objects':len(reuse),'all_final_links_fresh_policy':737,'products_reused':0,'archives':14,'generators_fresh_policy':3,'selected_native_tests':len(review['selected_native_names'])},'policy':{'literal_attached_D_unchanged':True,'standalone_D_refused':True,'sensitive_source_paths':sorted(sensitive),'source_sensitive_owner_count':len(extra_sensitive),'assets_byteidentical':assets_equal,'asset_files':len(assetsnew),'configured_graph_CMake_inputs_verified':config_equal,'actual_current_source_and_header_SHA_per_reused_owner':True,'no_guessed_generated_aliases':True,'all_final_links_fresh_for_path_embedded_RUNPATH_safety':True},'changed_project_paths':sorted(k for k in set(oldtree)|set(newtree)if oldtree.get(k)!=newtree.get(k)),'reason_counts':dict(sorted(reasons.items())),'route_proof':{'path':str(routefile),'sha256':hashlib.sha256(compressed).hexdigest(),'raw_sha256':hashlib.sha256(raw).hexdigest(),'owner_count':len(rows)},'pins_count':len(pins),'initial_final_pin_drift':[],'seconds':time.monotonic()-started,'build_authorization':'No execution authorized by this preview; root final freeze and plan review mandatory'}
write(OUT/'status.json',report);write(OUT/'pins.json',pins);print(json.dumps(report,indent=2))
