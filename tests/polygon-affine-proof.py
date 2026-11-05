#!/usr/bin/env python3
"""Bounded seven-whole-TU incremental proof; never copies/modifies an archive."""
import hashlib,json,os,shlex,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OLD=Path('/home/blux/de-xr/dome-sculpt/artifacts/authoring-next36-cutout-306')
OUT=ROOT/'artifacts/affine-proof';OUT.mkdir(parents=True,exist_ok=True)
CAP=5*2**28 # coordinator-approved cumulative1.25GiB
FLOOR=20*2**30
PRODUCT=['document.cpp','document-polygon.cpp','polygon-authoring.cpp','polygon-conversion.cpp','polygon-render.cpp','app.cpp','xr.cpp']
CORE=PRODUCT[:4]
RECEIPT=json.loads((ROOT/'docs/validation/VRM1-EXPRESSION-PREFLIGHT.json').read_text())
FRONT=json.loads(Path('/home/blux/de-xr/dome-sculpt/artifacts/retopo-affine-10944ab/artifacts/app-plan/frontend-closure.json').read_text())

def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  while b:=f.read(2**20):h.update(b)
 return h.hexdigest()
def allocated():
 return sum(p.stat().st_blocks*512 for p in ROOT.rglob('*') if p.is_file() and not p.is_symlink())
def free():
 s=os.statvfs(ROOT);return s.f_bavail*s.f_frsize
def guard(prospective=0):
 a=allocated();f=free()
 if a+prospective>CAP or f-prospective<FLOOR:raise RuntimeError(f'Allocation/free guard: allocated={a}, prospective={prospective}, free={f}')
 return dict(allocated=a,free=f,prospective=prospective)
def sources():
 paths=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard'],cwd=ROOT,text=True).splitlines()
 return {p:sha(ROOT/p) for p in paths if (ROOT/p).is_file() and p.startswith(('src/','include/','tests/','third_party/'))}
def save(p,d):p.write_text(json.dumps(d,indent=2)+'\n')
def run_step(state,name,cmd,env=None,prospective=0):
 pre=guard(prospective);start=time.time();log=Path(state['directory'])/(name+'.log')
 maps={}
 with log.open('wb') as f:
  proc=subprocess.Popen(cmd,cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT)
  while proc.poll() is None:
   if env is not None:
    try:
     for line in Path(f'/proc/{proc.pid}/maps').read_text().splitlines():
      parts=line.split(None,5)
      if len(parts)==6 and parts[5].startswith('/'):
       path=Path(parts[5])
       if path.is_file() and str(path) not in maps:maps[str(path)]=sha(path)
    except (FileNotFoundError,ProcessLookupError):pass
   time.sleep(.01)
  code=proc.wait()
 row=dict(name=name,command=[str(x) for x in cmd],exit=code,seconds=time.time()-start,log=str(log),before=pre,after=guard(),actual_maps=maps)
 if maps:
  drift=[path for path,h in maps.items() if not Path(path).exists() or sha(path)!=h]
  row['actual_map_drift']=drift
  if drift:row['exit']=1
 state['steps'].append(row);save(Path(state['directory'])/'status.json',state)
 if row['exit']:raise RuntimeError(f"{name} failed with {row['exit']}; preserved {log}")
 return row

def profile(which,tag=""):
 directory=OUT/(which+tag);directory.mkdir(exist_ok=False)
 state=dict(state='running',profile=which,directory=str(directory),steps=[],pid=os.getpid(),cap_bytes=CAP,floor_bytes=FLOOR)
 before=sources();save(directory/'source-before.json',before)
 db=json.loads((OLD/f'build-next36-{which}/compile_commands.json').read_text())
 oldbuild=OLD/f'build-next36-{which}'
 original=next(s['command'] for s in RECEIPT['commands']['native-app-'+which]['steps'] if s['name']=='link-dome-sculpt')
 oldfront={str(Path(o['path'])) for o in FRONT[which]['objects']}
 def absolute(a):
  if a.endswith(('.o','.a')) and not a.startswith('-'):
   p=Path(a);return str(p if p.is_absolute() else oldbuild/p)
  return a
 original=[absolute(a) for a in original]
 protected={str(Path(a)):sha(a) for a in original if a.endswith(('.o','.a')) and not a.startswith('-')}
 # Dynamic SDK/renderer libraries used by the immutable app also stay pinned.
 save(directory/'immutable-link-inputs.json',protected)
 state['initial_space']=guard()
 try:
  objects={}
  for source in ['src/'+s for s in PRODUCT]+['tests/polygon-affine.cpp']:
   entry=next((x for x in db if x['file'].endswith('/'+source)),None)
   if entry is None:entry=next(x for x in db if x['file'].endswith('/tests/polygon-document.cpp'))
   argv=entry.get('arguments') or shlex.split(entry['command']);cmd=[];skip=False
   for a in argv:
    if skip:skip=False;continue
    if a in ('-o','-MF','-MT','-MQ'):skip=True;continue
    if a in ('-c','-MD','-MMD','-MP') or a==entry['file']:continue
    cmd.append(a.replace(str(OLD),str(ROOT)))
   target=directory/(Path(source).name+'.o');dep=target.with_suffix('.d')
   cmd+=['-I'+str(ROOT/'src'),'-I'+str(ROOT/'tests'),'-MMD','-MF',str(dep),'-o',str(target),'-c',str(ROOT/source)]
   run_step(state,'compile-'+Path(source).name,cmd,prospective=(80 if which=='sanitize' else 8)*2**20)
   objects[Path(source).name]=str(target)
  # Preserve current immutable C179/main10944 inputs; replace whole TUs before
  # their old archive members. No ar extraction/repack or archive copy occurs.
  for kind in ('app','consumer'):
   cmd=[];skip=False
   for a in original:
    if skip:skip=False;continue
    if a=='-o':skip=True;continue
    if a in oldfront:
     if kind=='app':cmd.append(objects['app.cpp'] if a.endswith('/src/app.cpp.o') else a)
     continue
    cmd.append(a)
   objnames=PRODUCT[:-2]+['xr.cpp'] if kind=='app' else CORE
   # app.cpp was replaced in situ among original frontend objects.
   if kind=='consumer':objnames=CORE+['polygon-affine.cpp']
   cmd=cmd[:1]+[objects[s] for s in objnames]+cmd[1:]
   exe=directory/('dome-sculpt' if kind=='app' else 'polygon-affine')
   cmd+=['-Wl,-t,-t','-o',str(exe)]
   run_step(state,'link-'+kind,cmd,prospective=(470 if which=='sanitize' else 24)*2**20)
   trace=(directory/('link-'+kind+'.log')).read_text()
   excluded=PRODUCT[:-2]+['xr.cpp'] if kind=='app' else CORE
   bad=[s for s in excluded if any((')'+s+'.o') in line or ('.a('+s+'.o)') in line for line in trace.splitlines())]
   assert not bad,('old overridden archive member selected',bad)
  env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  toolkit='/home/blux/de-xr/DomeD/artifacts/fltk-poll-final/artifacts/toolkit-poll-final-'+which
  bitmap='/home/blux/de-xr/DomeD/artifacts/cairo-shm-drain-f797/artifacts/bitmap-runtime'
  env['LD_LIBRARY_PATH']=toolkit+'/lib:'+bitmap+'/lib'
  private=directory/'private';private.mkdir();env.update(HOME=str(private),XDG_STATE_HOME=str(private),XDG_CONFIG_HOME=str(private),XDG_CACHE_HOME=str(private))
  for kind in ('dome-sculpt','polygon-affine'):
   deps=subprocess.run(['ldd',str(directory/kind)],env=env,check=True,text=True,capture_output=True).stdout
   (directory/(kind+'-ldd.txt')).write_text(deps)
   for line in deps.splitlines():
    for word in line.split():
     path=Path(word)
     if word.startswith('/') and path.is_file():protected[word]=sha(path)
  save(directory/'immutable-link-inputs.json',protected)
  run_step(state,'app-version',[str(directory/'dome-sculpt'),'--version'],env)
  run_step(state,'basic',[str(directory/'polygon-affine'),'--out',str(directory/'native-output')],env)
  run_step(state,'controller',[str(directory/'polygon-affine'),'--controller'],env)
  run_step(state,'faults',[str(directory/'polygon-affine'),'--faults'],env)
  state['exit']=0
 except BaseException as e:
  state['exit']=1;state['error']=repr(e)
 finally:
  after=sources();save(directory/'source-after.json',after)
  state['source_drift']=[p for p in before.keys()|after.keys() if before.get(p)!=after.get(p)]
  state['immutable_input_drift']=[p for p,h in protected.items() if sha(p)!=h]
  state['artifacts']={str(p.relative_to(directory)):sha(p) for p in directory.rglob('*') if p.is_file() and p.name not in ('status.json',)}
  state['space']=guard();state['state']='terminal'
  if state['source_drift'] or state['immutable_input_drift']:state['exit']=1
  save(directory/'status.json',state)
 return state['exit']
if __name__=='__main__':
 if len(sys.argv) not in (2,3) or sys.argv[1] not in ('release','sanitize'):raise SystemExit('Use release first, then sanitize after review; optional unique suffix preserves earlier attempts')
 tag=sys.argv[2] if len(sys.argv)==3 else ''
 if tag and (not tag.startswith('-') or '/' in tag or '..' in tag):raise SystemExit('Suffix must be a simple -tag')
 raise SystemExit(profile(sys.argv[1],tag))
