#!/usr/bin/env python3
"""Fresh owning-ABI phase-one build. Never reuse earlier-layout archives."""
import hashlib,json,os,pathlib,shutil,subprocess,sys,time
root=pathlib.Path(__file__).resolve().parents[1]
profile=sys.argv[1]
assert profile in ('release','sanitize')
out=root/'artifacts'/'refinement38-proof'/profile
out.mkdir(parents=True,exist_ok=False)
build=root/('build-owning38-'+profile)
assert not build.exists(), 'An existing build must be inspected, never restarted here'
sdk=pathlib.Path('/home/blux/de-xr/DomeD/artifacts/fltk-poll-final/artifacts/toolkit-poll-final-'+profile)
bitmap=pathlib.Path('/home/blux/de-xr/DomeD/artifacts/cairo-shm-drain-f797/artifacts/bitmap-runtime')
env=os.environ.copy();env.update(PKG_CONFIG_PATH=str(bitmap/'lib/pkgconfig'),LD_LIBRARY_PATH=str(sdk/'lib')+':'+str(bitmap/'lib'),LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',EGL_PLATFORM='surfaceless',MESA_SHADER_CACHE_DIR=str(out/'mesa-cache'),ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
tracked=subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0')
def digest(p):return hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
inputs={str(root/p):digest(root/p) for p in tracked if p and (root/p).is_file()}
for prefix in (sdk,bitmap):
 for p in prefix.rglob('*'):
  if p.is_file():inputs[str(p)]=digest(p)
(out/'before.json').write_text(json.dumps(inputs,sort_keys=True,indent=2)+'\n')
status={'profile':profile,'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'source_inputs':len(tracked)-1,'protected_inputs':len(inputs),'free_before':shutil.disk_usage(root).free,'steps':[]}
def save(): (out/'status.json').write_text(json.dumps(status,indent=2)+'\n')
def run(name,cmd):
 start=time.monotonic();print(name,flush=True)
 with (out/(name+'.log')).open('w') as log: result=subprocess.run(cmd,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
 status['steps'].append({'name':name,'command':cmd,'exit':result.returncode,'seconds':time.monotonic()-start});save()
 print(name,'exit',result.returncode,flush=True)
 return result.returncode
configure=['cmake','-S',str(root),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DBUILD_TESTING=ON','-DDOME_BUILD_UI=ON','-DDOME_BUILD_XR=ON','-DDOME_COMPUTE_BACKEND=CPU','-DFLTK_DIR='+str(sdk/'share/fltk'),'-U*DOME_BITMAP_RUNTIME*','-U*NATIVEGL*','-U*RENDERFONTS*']
if profile=='sanitize':
 flags='-O1 -g1 -DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer'
 configure+=['-DCMAKE_C_FLAGS_RELEASE='+flags,'-DCMAKE_CXX_FLAGS_RELEASE='+flags]
targets=['dome-sculpt','dome-sculpt-refinement38-geometry-test','dome-sculpt-paint-sampling-test','dome-sculpt-normal-frame-archive-test','dome-sculpt-scene-file-test','dome-sculpt-key-mirror-refinement-test']
result=run('configure',configure)
if not result:result=run('build',['nice','-n','15','cmake','--build',str(build),'-j2','--target',*targets])
if not result:
 binaries={str(build/t):digest(build/t) for t in targets}
 binaries.update({str(p):digest(p) for p in build.glob('*.a')})
 (out/'binaries-before.json').write_text(json.dumps(binaries,sort_keys=True,indent=2)+'\n')
 result=run('tests',['nice','-n','15','ctest','--test-dir',str(build),'--output-on-failure','-R','^(refinement38-geometry|paint-sampling|normal-frame-archive|scene-file|key-mirror-refinement)$','-j1'])
 status['artifact_drift']=[p for p,h in binaries.items() if digest(pathlib.Path(p))!=h]
status['source_drift']=[p for p,h in inputs.items() if not pathlib.Path(p).is_file() or digest(pathlib.Path(p))!=h]
status['free_after']=shutil.disk_usage(root).free
status['build_allocated_bytes']=sum(p.stat().st_blocks*512 for p in build.rglob('*') if p.is_file()) if build.exists() else 0
status['result']=result;save();print(json.dumps({k:v for k,v in status.items() if k!='steps'}),flush=True)
sys.exit(result or bool(status['source_drift']) or bool(status.get('artifact_drift')))
