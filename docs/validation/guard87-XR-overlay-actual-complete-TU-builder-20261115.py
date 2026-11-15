"""One complete current test-runtime TU and shared link; no product/test execution."""
import hashlib,json,os,shlex,subprocess,time
from pathlib import Path
R=Path('/home/blux/de-xr/dome-sculpt');P=Path(__file__).resolve().parent;S=R/'artifacts/source8-runtime-report-object-20261115';BASE=R/'artifacts/authoring-paused-uv-domain-20261112';F=R/'artifacts/current-toolchain-guard87-release-20261115/release-3d1643-current-fresh-v2-audit/plan.json';EXPECTED_PLAN='0d03a29a1d2251d1186cb8a9c94f22227566c86664245c51b050a66e458b7dec'
cache={}
def sha(p):
 p=Path(p);s=p.stat();key=(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)
 if key not in cache:
  with p.open('rb')as f:cache[key]=hashlib.file_digest(f,'sha256').hexdigest()
 return cache[key]
def write(p,j):p.write_text(json.dumps(j,indent=2)+'\n')
def verify(pins):return[p for p,h in pins.items()if not Path(p).is_file()or sha(p)!=h]
def symbols(p):
 text=subprocess.check_output(['nm','-D','--defined-only','--format=posix',str(p)],text=True);return sorted(' '.join(line.split()[0:2])for line in text.splitlines())
def soname(p):
 text=subprocess.check_output(['readelf','-d',str(p)],text=True);return[line.split('Library soname: ',1)[1]for line in text.splitlines()if'Library soname: 'in line]
def deps(p):
 text=p.read_text().replace('\\\n',' ');target,body=text.split(':',1);return shlex.split(target),shlex.split(body)
assert sha(F)==EXPECTED_PLAN;j=json.loads(F.read_text());assert j['source']=='3d1643827dd56e1b2ef1d57b8a9c6ff1e4211a2c';head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=S,text=True).strip();assert head.startswith('c54f66f8')and not subprocess.check_output(['git','status','--porcelain'],cwd=S,text=True)
changes=subprocess.check_output(['git','diff','--name-only',j['source'],head],cwd=S,text=True).splitlines();assert changes==['tests/source8-public-runtime.cpp']
pins=dict(j['pins']);pins[str(F)]=EXPECTED_PLAN;pins[str(Path(__file__).resolve())]=sha(__file__)
for name in subprocess.check_output(['git','ls-files','-z'],cwd=S,text=True).split('\0'):
 if not name:continue
 f=S/name
 if name!='tests/source8-public-runtime.cpp':assert sha(f)==sha(BASE/name),('Unexpected child source/header change',name)
 pins[str(f)]=sha(f)
assert not verify(pins);write(P/'prebuild-inputs.json',pins)
key='CMakeFiles/dome-sculpt-source8-public-runtime.dir/tests/source8-public-runtime.cpp.o';original=j['objects'][key]['command'];command=list(original);obj=P/'source8-public-runtime.cpp.o';dep=P/'source8-public-runtime.cpp.o.d';lib=P/'libdome-sculpt-source8-public-runtime.so';linkdep=P/'source8-public-runtime-link.d'
for flag,path in [('-MT',obj),('-MF',dep),('-o',obj)]:command[command.index(flag)+1]=str(path)
command[command.index('-c')+1]=str(S/'tests/source8-public-runtime.cpp');assert[x for x in command if x.startswith('-D')]==[x for x in original if x.startswith('-D')]
raw=j['links'][lib.name]['command'];assert raw.startswith(': && ')and raw.endswith(' && :');link=shlex.split(raw[5:-5]);link=[str(obj)if token==key else '-Wl,--dependency-file='+str(linkdep)if token.startswith('-Wl,--dependency-file=')else token for token in link];link[link.index('-o')+1]=str(lib)
plan={'scope':__doc__,'application_source':j['source'],'test_runtime_source':head,'changed_file':changes[0],'changed_file_sha256':sha(S/changes[0]),'owning_plan':{'path':str(F),'sha256':EXPECTED_PLAN},'original_flags_command':original,'complete_TU_command':command,'fresh_shared_link_command':link,'literal_defines_unchanged':True,'header_policy':'Every child header/source except the sole changed test TU matches frozen3d bytes. Actual fresh -MD/-MF closure must match prebuild expected SHA. No header/ABI fabrication.','pins_sha256':sha(P/'prebuild-inputs.json')};write(P/'overlay-plan.json',plan)
state={'state':'running','exit':1,'scope':__doc__,'application_source':j['source'],'test_runtime_source':head,'steps':[]}
def save():write(P/'status.json',state)
env=dict(os.environ,LD_LIBRARY_PATH=str(R/'artifacts/sdk-20261111/toolkit-release/lib')+':/opt/fltk-vr/bdf-runtime/lib:/usr/lib64');env.pop('LD_PRELOAD',None)
try:
 for name,args in [('compile-complete-test-runtime',command),('link-current-test-runtime',link)]:
  row={'name':name,'command':args,'started':time.time()};state['steps'].append(row);save()
  with(P/(name+'.log')).open('w')as log:c=subprocess.run(args,cwd=P,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
  row.update(exit=c.returncode,seconds=time.time()-row['started'],log_sha256=sha(P/(name+'.log')));save();assert c.returncode==0,(name,c.returncode)
 target,names=deps(dep);assert target==[str(obj)];entries=[]
 for name in names:
  f=Path(name);f=f if f.is_absolute()else P/f;assert str(f)in pins and sha(f)==pins[str(f)],('Unpinned/changed actual fresh dependency',str(f));entries.append({'path':str(f),'sha256':pins[str(f)]})
 state['actual_complete_TU_dependencies']={'path':str(dep),'sha256':sha(dep),'count':len(entries),'entries':entries}
 old=Path(j['destination'])/lib.name;state['ABI_contract']={'expected_SONAME':soname(old),'actual_SONAME':soname(lib),'expected_dynamic_exports':symbols(old),'actual_dynamic_exports':symbols(lib)};assert state['ABI_contract']['expected_SONAME']==state['ABI_contract']['actual_SONAME']==['[libdome-sculpt-source8-public-runtime.so]'];assert state['ABI_contract']['expected_dynamic_exports']==state['ABI_contract']['actual_dynamic_exports']
 state['fresh_test_runtime']={'path':str(lib),'sha256':sha(lib),'bytes':lib.stat().st_size};state['object']={'path':str(obj),'sha256':sha(obj)};state['link_dependency_file']={'path':str(linkdep),'sha256':sha(linkdep)};state['exit']=0
except BaseException as e:state['error']=repr(e)
finally:
 state.update(state='terminal',input_drift=verify(pins));state['exit']=state['exit']or int(bool(state['input_drift']));save();print(json.dumps({k:state.get(k)for k in ['state','exit','application_source','test_runtime_source','error','input_drift','fresh_test_runtime']}),flush=True)
raise SystemExit(state['exit'])
