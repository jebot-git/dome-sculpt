#!/usr/bin/env python3
"""Approved lightweight followup; production remains exact263c753/e0e9de5."""
import hashlib,json,os,resource,runpy,shlex,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
A=runpy.run_path(str(ROOT/'tests/polygon-affine-proof.py'))
OUT=ROOT/'artifacts/followup-proof';OUT.mkdir(exist_ok=True)
def sha(p):return A['sha'](p)
def save(p,d):p.write_text(json.dumps(d,indent=2)+'\n')
def profile(which,tag=""):
 directory=OUT/(which+tag);directory.mkdir(exist_ok=False)
 state=dict(state='running',profile=which,directory=str(directory),steps=[],pid=os.getpid(),scope='Test-only standalone renderer; Release-only Scene producer/observer; production remains263c753')
 before=A['sources']();save(directory/'source-before.json',before)
 old=A['OLD'];oldbuild=old/f'build-next36-{which}'
 db=json.loads((oldbuild/'compile_commands.json').read_text())
 entry=next(x for x in db if x['file'].endswith('/tests/polygon-render.cpp'))
 cmd=[];skip=False
 for a in shlex.split(entry['command']):
  if skip:skip=False;continue
  if a in ('-o','-MF','-MT','-MQ'):skip=True;continue
  if a in ('-c','-MD','-MMD','-MP') or a==entry['file']:continue
  cmd.append(a.replace(str(old),str(ROOT)))
 if which=='sanitize':cmd+=['-DDOME_AFFINE_PREVIEW_ONLY=1']
 obj=directory/'polygon-affine-render.cpp.o';cmd+=['-I'+str(ROOT/'src'),'-I'+str(ROOT/'tests'),'-MMD','-MF',str(directory/'consumer.d'),'-o',str(obj),'-c',str(ROOT/'tests/polygon-affine-render.cpp')]
 original=next(s['command'] for s in A['RECEIPT']['commands']['native-app-'+which]['steps'] if s['name']=='link-dome-sculpt')
 front={str(Path(x['path'])) for x in A['FRONT'][which]['objects']};link=[];skip=False
 for arg in original:
  if skip:skip=False;continue
  if arg=='-o':skip=True;continue
  a=arg
  if a.endswith(('.o','.a')) and not a.startswith('-'):
   p=Path(a);a=str(p if p.is_absolute() else oldbuild/p)
  if a in front:continue
  link.append(a)
 overridden=[ROOT/f'artifacts/affine-proof/{which}-capability/{s}.o' for s in ('polygon-authoring.cpp','polygon-conversion.cpp','polygon-render.cpp')]
 exe=directory/'polygon-affine-render'
 link=link[:1]+[str(obj),*[str(p) for p in overridden]]+link[1:]+['-Wl,--compress-debug-sections=zstd','-Wl,-t,-t',*[f'-Wl,--wrap={n}' for n in ('glBufferData','glBufferSubData','glGetBufferSubData','glMapBuffer','glMapBufferRange','glReadPixels')],'-o',str(exe)]
 protected={a:sha(a) for a in link if a.endswith(('.o','.a')) and a!=str(obj) and not a.startswith('-')}
 save(directory/'immutable-inputs.json',protected)
 try:
  A['run_step'](state,'compile',cmd,prospective=(32 if which=='sanitize' else 8)*2**20)
  estimate=(96 if which=='sanitize' else 24)*2**20
  pre=A['guard'](estimate);start=time.time()
  def limit():resource.setrlimit(resource.RLIMIT_FSIZE,(estimate,estimate))
  with (directory/'link.log').open('wb') as f:r=subprocess.run(link,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,preexec_fn=limit)
  state['steps'].append(dict(name='link',command=link,exit=r.returncode,seconds=time.time()-start,before=pre,after=A['guard'](),hard_output_limit_bytes=estimate));save(directory/'status.json',state)
  if r.returncode:raise RuntimeError('Link failed; original output/log retained')
  trace=(directory/'link.log').read_text();bad=[]
  for name in ('document.cpp.o','document-polygon.cpp.o','polygon-authoring.cpp.o','polygon-render.cpp.o','polygon-conversion.cpp.o','render.cpp.o','xr.cpp.o'):
   if any((')'+name) in line or '.a('+name+')' in line for line in trace.splitlines()):bad.append(name)
  assert not bad,('Unexpected changed/heavy old member selected',bad)
  env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',EGL_PLATFORM='surfaceless',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  toolkit='/home/blux/de-xr/DomeD/artifacts/fltk-poll-final/artifacts/toolkit-poll-final-'+which
  bitmap='/home/blux/de-xr/DomeD/artifacts/cairo-shm-drain-f797/artifacts/bitmap-runtime'
  env['LD_LIBRARY_PATH']=toolkit+'/lib:'+bitmap+'/lib';private=directory/'private';private.mkdir()
  env.update(HOME=str(private),XDG_STATE_HOME=str(private),XDG_CONFIG_HOME=str(private),XDG_CACHE_HOME=str(private),MESA_SHADER_CACHE_DIR=str(private/'mesa'))
  deps=subprocess.run(['ldd',str(exe)],env=env,check=True,text=True,capture_output=True).stdout;(directory/'ldd.txt').write_text(deps)
  for line in deps.splitlines():
   for a in line.split():
    if a.startswith('/') and Path(a).is_file():protected[a]=sha(a)
  save(directory/'immutable-inputs.json',protected)
  if which=='release':A['run_step'](state,'fixtures',[str(exe),'--fixtures',str(directory/'fixtures')],env)
  A['run_step'](state,'render',[str(exe),str(directory/'images')],env)
  state['exit']=0
 except BaseException as e:state['exit']=1;state['error']=repr(e)
 finally:
  after=A['sources']();save(directory/'source-after.json',after)
  state['source_drift']=[p for p in before.keys()|after.keys() if before.get(p)!=after.get(p)]
  state['immutable_input_drift']=[p for p,h in protected.items() if sha(p)!=h]
  if state['source_drift'] or state['immutable_input_drift']:state['exit']=1
  state['space']=A['guard']();state['state']='terminal';state['artifacts']={str(p.relative_to(directory)):sha(p) for p in directory.rglob('*') if p.is_file() and p.name!='status.json'};save(directory/'status.json',state)
 return state['exit']
if __name__=='__main__':
 if len(sys.argv) not in (2,3) or sys.argv[1] not in ('release','sanitize'):raise SystemExit('Use release, then sanitize; optional unique -tag')
 tag=sys.argv[2] if len(sys.argv)==3 else ''
 if tag and (not tag.startswith('-') or '/' in tag or '..' in tag):raise SystemExit('Invalid unique tag')
 raise SystemExit(profile(sys.argv[1],tag))
