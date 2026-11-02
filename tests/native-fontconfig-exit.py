#!/usr/bin/env python3
"""Native process ownership: synthetic-loader XR and desktop capture exits."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_NATIVE_FONT_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_NATIVE_FONT_BUS':'1'})
mode=os.environ.get('DOME_NATIVE_FONT_MODE','all');assert mode in ('all','desktop','xr');binary,runtime,out=map(lambda v:Path(v).resolve(),sys.argv[1:4]);lifecycle=Path(sys.argv[4]).resolve() if len(sys.argv)>4 else None;out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True);(out/'report.json').write_text(json.dumps({'checks':checks,'complete':False},indent=2)+'\n');assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-native-font-') as home:
 env={**os.environ,'HOME':home,'XDG_CONFIG_HOME':home+'/config','XDG_DATA_HOME':home+'/data','XDG_STATE_HOME':home+'/state','XDG_CACHE_HOME':home+'/cache','XDG_RUNTIME_DIR':home,'XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','SPNAV_SOCKET':home+'/unavailable.sock'}
 for k in ('DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','FLTK_BACKEND'):env.pop(k,None)
 manifest=Path(home,'runtime.json');manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}));env['XR_RUNTIME_JSON']=str(manifest)
 def run(name,args,extra=None):
  with (out/(name+'.log')).open('w') as log:return subprocess.run([str(binary),*args],env={**env,**(extra or {})},stdout=log,stderr=subprocess.STDOUT,timeout=90)
 if mode!='desktop':
  project=out/'standalone.dsculpt';result=run('standalone',['--xr','--frames','16','--save',str(project)],{'DOME_XR_FIXTURE_REPORT':str(out/'standalone-runtime.json')});check(result.returncode==0 and project.is_file(),'Standalone native XR drains its GL resources, appearance monitor and Document before process Fontconfig cleanup')
  report=json.loads((out/'standalone-runtime.json').read_text());(out/'runtime-detail.json').write_text(json.dumps(report,indent=2)+'\n')
  check(report.get('frames_ended',0)>=16 and report.get('sessions_destroyed',0)==1,'Real OpenXR loader executes the bounded synthetic tracking session before clean exit')
  with (out/'inspect.log').open('w') as log:result=subprocess.run([str(binary),'--project-info',str(project)],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=45)
  check(result.returncode==0,'Headless project-info remains usable without entering the native process font scope')
 if mode!='xr':
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');server=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1000x800x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);os.close(wr)
  try:
   with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
   image=out/'smoke.ppm';result=run('smoke',['--smoke-test',str(image)],{'DISPLAY':screen,'FLTK_BACKEND':'x11'})
   check(result.returncode==0 and image.is_file() and image.stat().st_size>1000,'Native desktop smoke capture preserves its image and returns through normal App/font teardown')
   result=run('capture-error',['--smoke-test',str(Path(home,'nonexistent/capture.ppm'))],{'DISPLAY':screen,'FLTK_BACKEND':'x11'})
   check(result.returncode==1 and 'Cannot write capture' in (out/'capture-error.log').read_text(),'Capture write failure unwinds native App and process font ownership with its existing error status')
   if lifecycle:
    with (out/'context-lifetime.log').open('w') as log:result=subprocess.run([str(lifecycle)],env={**env,'DISPLAY':screen,'FLTK_BACKEND':'x11'},stdout=log,stderr=subprocess.STDOUT,timeout=90)
    check(result.returncode==0,'Actual native Canvas safely repeats hide/show, redraw, partial initialization and worker teardown')
  finally:
   if server.poll() is None:server.terminate()
   try:server.wait(timeout=5)
   except subprocess.TimeoutExpired:server.kill();server.wait()
 (out/'report.json').write_text(json.dumps({'checks':checks,'complete':True,'scope':mode,'synthetic_tracking':mode!='desktop','physical_headset':False,'sanitizer_suppressions':False},indent=2)+'\n')
