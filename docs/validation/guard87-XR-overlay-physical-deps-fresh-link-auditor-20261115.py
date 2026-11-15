"""Audit the actual complete V1 TU and first genuine linker dependency rule, then freshly relink.
V1 lexical dependency-key failure stays failed. No application/runtime execution.
"""
import hashlib,json,os,shlex,subprocess,time
from pathlib import Path
R=Path('/home/blux/de-xr/dome-sculpt');P=Path(__file__).resolve().parent;OLD=R/'artifacts/source8-runtime-report-object-build-20261115';S=R/'artifacts/source8-runtime-report-object-20261115'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write(p,j):p.write_text(json.dumps(j,indent=2)+'\n')
def verify(pins):return[p for p,h in pins.items()if not Path(p).is_file()or sha(p)!=h]
def depnames(p):
 target,body=p.read_text().replace('\\\n',' ').splitlines()[0].split(':',1);return shlex.split(target),shlex.split(body)
def exports(p):return sorted(' '.join(line.split()[0:2])for line in subprocess.check_output(['nm','-D','--defined-only','--format=posix',str(p)],text=True).splitlines())
def dynamic(p):
 text=subprocess.check_output(['readelf','-d',str(p)],text=True);return [line.split('Library soname: ',1)[1]for line in text.splitlines()if'Library soname: 'in line],sorted(line.split('Shared library: ',1)[1]for line in text.splitlines()if'Shared library: 'in line)
status=OLD/'status.json';old=json.loads(status.read_text());assert old['state']=='terminal'and old['exit']==1 and not old['input_drift'];assert [x['exit']for x in old['steps']]==[0,0]and'Unpinned/changed actual fresh dependency'in old['error'];plan=OLD/'overlay-plan.json';q=json.loads(plan.read_text());inputs=OLD/'prebuild-inputs.json';assert sha(inputs)==q['pins_sha256'];pins=json.loads(inputs.read_text());assert not verify(pins)
for f in [status,plan,inputs,Path(__file__).resolve()]:pins[str(f)]=sha(f)
object_file=OLD/'source8-public-runtime.cpp.o';compiledep=OLD/'source8-public-runtime.cpp.o.d';pins[str(object_file)]=sha(object_file);pins[str(compiledep)]=sha(compiledep)
physical={}
for name,value in pins.items():
 key=str(Path(name).resolve());assert key not in physical or physical[key]==value;physical[key]=value
_,names=depnames(compiledep);records=[]
for name in names:
 p=Path(name);p=p if p.is_absolute()else OLD/p;actual=str(p.resolve());assert actual in physical and sha(p)==physical[actual],('Actual fresh compiler dependency changed/not prepinned',str(p),actual);records.append({'actual_depfile_spelling':str(p),'resolved_physical':actual,'expected_sha256':physical[actual]})
state={'state':'preparing','exit':1,'scope':__doc__,'application_source':q['application_source'],'test_runtime_source':q['test_runtime_source'],'whole_TU_provider':{'object':str(object_file),'sha256':sha(object_file),'actual_complete_compile_step':old['steps'][0],'actual_depfile':str(compiledep),'actual_depfile_sha256':sha(compiledep),'actual_dependency_count':len(records),'physical_dependency_proofs':records,'original_lexical_harness_failure':{'path':str(status),'sha256':sha(status),'exit':1}},'steps':[]}
# Capture actual V1 linker dependency paths before a NEW final V2 link. Nothing
# about historical startup-object bytes is invented or rewritten into V1 pins.
_,linknames=depnames(OLD/'source8-public-runtime-link.d')
for name in linknames:
 p=Path(name);p=p if p.is_absolute()else OLD/p;assert p.is_file();pins[str(p)]=sha(p);pins[str(p.resolve())]=sha(p)
link=list(q['fresh_shared_link_command']);lib=P/'libdome-sculpt-source8-public-runtime.so';dep=P/'source8-public-runtime-link.d';link[link.index('-o')+1]=str(lib);link=[('-Wl,--dependency-file='+str(dep))if token.startswith('-Wl,--dependency-file=')else token for token in link]
write(P/'prelink-inputs.json',pins);write(P/'fresh-link-plan.json',{'application_source':q['application_source'],'test_runtime_source':q['test_runtime_source'],'actual_complete_TU_provider':state['whole_TU_provider'],'fresh_link_command':link,'original_overlay_plan':{'path':str(plan),'sha256':sha(plan)},'prelink_inputs_sha256':sha(P/'prelink-inputs.json'),'literal_defines_unchanged':True});write(P/'status.json',state)
try:
 with(P/'fresh-link.log').open('w')as f:c=subprocess.run(link,cwd=P,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 state['steps'].append({'name':'fresh-current-shared-link','command':link,'exit':c.returncode,'log_sha256':sha(P/'fresh-link.log')});assert c.returncode==0
 physical={str(Path(name).resolve()):h for name,h in pins.items()};_,current_linknames=depnames(dep);ldrecords=[]
 for name in current_linknames:
  p=Path(name);p=p if p.is_absolute()else P/p;real=str(p.resolve());assert real in physical and sha(p)==physical[real],('Actual final link input missing/changed from prelink boundary',str(p));ldrecords.append({'path':str(p),'resolved':real,'sha256':physical[real]})
 own=json.loads(Path(q['owning_plan']['path']).read_text());baseline=Path(own['destination'])/lib.name;old_soname,old_needed=dynamic(baseline);new_soname,new_needed=dynamic(lib);old_exports=exports(baseline);new_exports=exports(lib);assert old_soname==new_soname==['[libdome-sculpt-source8-public-runtime.so]'];assert old_needed==new_needed;assert old_exports==new_exports
 state['ABI_contract']={'expected_SONAME':old_soname,'actual_SONAME':new_soname,'expected_NEEDED':old_needed,'actual_NEEDED':new_needed,'dynamic_exports_exact':True,'expected_exports':old_exports,'actual_exports':new_exports};state['actual_link_dependencies']={'path':str(dep),'sha256':sha(dep),'count':len(ldrecords),'records':ldrecords};state['test_runtime']={'path':str(lib),'sha256':sha(lib),'bytes':lib.stat().st_size};state['exit']=0
except BaseException as e:state['error']=repr(e)
finally:
 state.update(state='terminal',input_drift=verify(pins));state['exit']=state['exit']or int(bool(state['input_drift']));write(P/'status.json',state);print(json.dumps({k:state.get(k)for k in ['state','exit','application_source','test_runtime_source','error','input_drift','test_runtime']}),flush=True)
raise SystemExit(state['exit'])
