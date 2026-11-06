#!/usr/bin/env python3
"""Reviewed first Release cohort only. No full build or archive writes."""
import hashlib,json,os,resource,shlex,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
POLY=Path('/home/blux/de-xr/dome-sculpt/artifacts/polygon-affine-10944')
CAP=int(.2*2**30);FLOOR=20*2**30
OUT=ROOT/'artifacts/integration-proof'/('release'+(sys.argv[1] if len(sys.argv)>1 else ''))
OUT.mkdir(parents=True,exist_ok=False)
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  while b:=f.read(2**20):h.update(b)
 return h.hexdigest()
def save(name,value):(OUT/name).write_text(json.dumps(value,indent=2)+'\n')
def space(n=0):
 allocated=sum(p.stat().st_blocks*512 for p in ROOT.rglob('*') if p.is_file() and not p.is_symlink());s=os.statvfs(ROOT);free=s.f_bavail*s.f_frsize
 if allocated+n>CAP or free-n<FLOOR:raise RuntimeError(('Space guard',allocated,free,n))
 return dict(allocated=allocated,actual_free=free,prospective=n)
def sources():
 names=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard'],cwd=ROOT,text=True).splitlines()
 return {p:sha(ROOT/p) for p in names if (ROOT/p).is_file() and (p.startswith(('src/','include/','tests/','third_party/')) or p=='CMakeLists.txt')}
state=dict(state='running',pid=os.getpid(),head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),steps=[],scope='Fresh complete merged Document.cpp and whole new test; audited whole main5+polygon6 overrides before immutable C179; no partial functions or archive copies')
initial=sources();save('source-before.json',initial);protected={}
def step(name,cmd,env=None,prospective=0,hard_output=None):
 before=space(prospective);start=time.time();maps={}
 def limit():resource.setrlimit(resource.RLIMIT_FSIZE,(hard_output,hard_output))
 with (OUT/(name+'.log')).open('wb') as log:
  p=subprocess.Popen(cmd,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,preexec_fn=limit if hard_output else None)
  while p.poll() is None:
   if env:
    try:
     for row in Path(f'/proc/{p.pid}/maps').read_text().splitlines():
      fields=row.split(None,5)
      if len(fields)==6 and fields[5].startswith('/') and fields[5] not in maps:
       path=Path(fields[5])
       try:
        with path.open('rb') as f:elf=f.read(4)==b'\x7fELF'
        if elf:maps[str(path)]=sha(path)
       except OSError:pass
    except OSError:pass
   time.sleep(.01)
  code=p.wait()
 row=dict(name=name,command=[str(a) for a in cmd],exit=code,seconds=time.time()-start,before=before,after=space(),actual_ELF_maps=maps,actual_ELF_map_drift=[p for p,h in maps.items() if not Path(p).exists() or sha(p)!=h]);state['steps'].append(row);save('status.json',state)
 if code or row['actual_ELF_map_drift']:raise RuntimeError((name,code,row['actual_ELF_map_drift']))
 return row
try:
 layout=json.loads((ROOT/'artifacts/integration-plan/combined-layout.json').read_text());assert layout['exit']==0 and layout['equal_public_and_outer_offsets']
 audit=json.loads((ROOT/'artifacts/integration-plan/source-object-audit.json').read_text());assert all(v if isinstance(v,bool) else all(v.values()) for v in audit['closure'].values())
 # Compile commands are complete matching Release TU commands, source/header
 # paths rebased to this candidate. No standard flags or FP checks are removed.
 prior=json.loads((POLY/'artifacts/affine-proof/release-capability/status.json').read_text());objects={}
 for source,template in [('src/document.cpp','compile-document.cpp'),('tests/polygon-retopo-affine.cpp','compile-polygon-affine.cpp')]:
  oldcmd=next(x['command'] for x in prior['steps'] if x['name']==template);cmd=[];skip=False
  for a in oldcmd:
   if skip:skip=False;continue
   if a in ('-o','-MF','-MT','-MQ'):skip=True;continue
   if a in ('-c','-MD','-MMD','-MP') or a.endswith('/src/document.cpp') or a.endswith('/tests/polygon-affine.cpp'):continue
   cmd.append(a.replace(str(POLY),str(ROOT)))
  target=OUT/(Path(source).name+'.o');cmd+=['-MMD','-MF',str(target.with_suffix('.d')),'-o',str(target),'-c',str(ROOT/source)]
  step('compile-'+Path(source).name,cmd,prospective=8*2**20);objects[Path(source).name]=target
 receipt=json.loads((ROOT/'docs/validation/RETOPO-SEMANTIC-CLIENTS.json').read_text());old=next(x for x in receipt['status']['steps'] if x['name']=='link-ordinary-app' and x['profile']=='release');maincmd=old['command'];cwd=Path(old['cwd'])
 allobjects={str((Path(a) if Path(a).is_absolute() else cwd/a)) for a in maincmd if a.endswith('.o')}
 front={p for p in allobjects if '/CMakeFiles/dome-sculpt.dir/' in p};main_doc=next(p for p in allobjects if p.endswith('/document.cpp.o'))
 qualified=[POLY/f'artifacts/affine-proof/release-capability/{name}.o' for name in ('document-polygon.cpp','polygon-authoring.cpp','polygon-conversion.cpp','polygon-render.cpp','app.cpp','xr.cpp')]
 for kind in ('app','consumer'):
  link=[];skip=False
  for a in maincmd:
   if skip:skip=False;continue
   if a=='-o':skip=True;continue
   if a.endswith(('.o','.a')) and not a.startswith('-'):
    p=Path(a);a=str(p if p.is_absolute() else cwd/p)
   if a==main_doc or (a in front and (kind=='consumer' or a.endswith('/src/app.cpp.o'))):continue
   if '-Map' in a:raise RuntimeError('Refuse inherited output map path')
   link.append(a)
  providers=qualified if kind=='app' else qualified[:4]
  prefix=[objects['document.cpp']]+providers+([objects['polygon-retopo-affine.cpp']] if kind=='consumer' else [])
  link=link[:1]+[str(p) for p in prefix]+link[1:]+['-Wl,--compress-debug-sections=zstd','-Wl,-t,-t','-o',str(OUT/('dome-sculpt' if kind=='app' else 'polygon-retopo-affine'))]
  for a in link:
   if a.endswith(('.o','.a')) and not a.startswith('-'):protected[a]=sha(a)
  save('immutable-inputs.json',protected);step('link-'+kind,link,prospective=24*2**20,hard_output=24*2**20)
  trace=(OUT/('link-'+kind+'.log')).read_text()
  ban=('document.cpp.o','document-polygon.cpp.o','document-retopo.cpp.o','document-baking.cpp.o','retopo.cpp.o','retopo-authoring.cpp.o','render.cpp.o','polygon-authoring.cpp.o','polygon-conversion.cpp.o','polygon-render.cpp.o','xr.cpp.o')
  bad=[name for name in ban if any((')'+name) in row or '.a('+name+')' in row for row in trace.splitlines())];assert not bad,bad
 env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',MESA_SHADER_CACHE_DISABLE='1',LD_LIBRARY_PATH='/home/blux/de-xr/DomeD/artifacts/fltk-poll-final/artifacts/toolkit-poll-final-release/lib:/home/blux/de-xr/DomeD/artifacts/cairo-shm-drain-f797/artifacts/bitmap-runtime/lib');private=OUT/'private';private.mkdir();env.update(HOME=str(private),XDG_STATE_HOME=str(private),XDG_CONFIG_HOME=str(private),XDG_CACHE_HOME=str(private))
 step('app-version',[str(OUT/'dome-sculpt'),'--version'],env)
 step('combined-native',[str(OUT/'polygon-retopo-affine'),str(OUT/'native-output')],env,prospective=4*2**20)
 state['exit']=0
except BaseException as e:state.update(exit=1,error=repr(e))
finally:
 after=sources();save('source-after.json',after);state['source_drift']=[p for p in initial.keys()|after.keys() if initial.get(p)!=after.get(p)];state['input_drift']=[p for p,h in protected.items() if not Path(p).exists() or sha(p)!=h];state['space']=space();state['state']='terminal'
 if state['source_drift'] or state['input_drift']:state['exit']=1
 save('status.json',state)
print(json.dumps({k:v for k,v in state.items() if k!='steps'}));raise SystemExit(state['exit'])
