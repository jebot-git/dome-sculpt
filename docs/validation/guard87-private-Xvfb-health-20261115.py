"""Isolated Xvfb displayfd/X11/XTEST and actual private server closure; no app."""
import fcntl,json,os,select,signal,subprocess,time
from pathlib import Path
from Xlib import display,X,XK
from Xlib.ext import xtest
from xvfb_identity import sha,identity,certify
P=Path(__file__).resolve().parent;OUT=P/'health-v2';OUT.mkdir();prepared=P/'private-closure-prepared-v1.json';j=json.loads(prepared.read_text());pins=dict(j['pins']);pins[str(prepared)]=sha(prepared)
for f in [Path(__file__),P/'xvfb_identity.py',P.parents[1]/'artifacts/strict-arc-20261111/control/mount_identity.py']:pins[str(f)]=sha(f)
assert not [p for p,h in pins.items()if not Path(p).is_file()or sha(p)!=h]
state={'state':'preparing','exit':1,'scope':__doc__,'steps':[]};lease=Path('/tmp/dome-sculpt-arc-device.lock').open('a+');fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB);state['graphics_lease']={'owner_pid':os.getpid(),'acquired_epoch':time.time(),'before_server':True};child=None;owned={};d=None
try:
 env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
 for k in ['LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','XR_RUNTIME_JSON']:env.pop(k,None)
 env.update(LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2');rd,wr=os.pipe()
 with(OUT/'xvfb.log').open('w')as log:child=subprocess.Popen([str(P/'bin/Xvfb'),'-displayfd',str(wr),'-screen','0','1100x800x24','-nolisten','tcp'],env=env,pass_fds=(wr,),stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
 os.close(wr);state['wrapper_pid']=child.pid
 assert select.select([rd],[],[],10)[0],'No actual Xvfb displayfd within10s';text=os.read(rd,128).decode().strip();os.close(rd);assert text.isdigit(),('Actual displayfd is not a display number',repr(text));state['display']=':'+text
 d=display.Display(state['display']);screen=d.screen();assert(screen.width_in_pixels,screen.height_in_pixels)==(1100,800);assert d.query_extension('XTEST').present
 w=screen.root.create_window(0,0,64,64,0,screen.root_depth,X.InputOutput,X.CopyFromParent,background_pixel=screen.black_pixel,event_mask=X.KeyPressMask|X.KeyReleaseMask);w.map();w.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();code=d.keysym_to_keycode(XK.string_to_keysym('a'));assert code
 xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code);d.sync();events=[]
 while d.pending_events():events.append(d.next_event())
 assert any(e.type==X.KeyPress and e.detail==code for e in events),'No real XTEST key event received'
 todo=[child.pid];seen=set();proofs=[]
 while todo:
  pid=todo.pop()
  if pid in seen:continue
  seen.add(pid);ident=identity(pid)
  if not ident:continue
  owned[pid]=ident
  try:todo+=list(map(int,Path('/proc',str(pid),'task',str(pid),'children').read_text().split()))
  except(FileNotFoundError,ProcessLookupError):pass
  try:
   actual=Path('/proc',str(pid),'exe').stat();expected=(P/'deps/usr/bin/Xvfb').stat()
   if(actual.st_dev,actual.st_ino)==(expected.st_dev,expected.st_ino):proofs.append(certify(pid,pins,sealing=True))
  except(FileNotFoundError,ProcessLookupError):pass
 assert proofs,'Actual private Xvfb executable/maps never observed';state.update(actual_server_proofs=proofs,screen=[1100,800],actual_XTEST_key_received=True,exit=0);w.destroy();d.close();d=None
except BaseException as e:state.update(error=repr(e),exit=1)
finally:
 if d is not None:d.close()
 if child is not None:
  if child.poll()is None:
   try:os.killpg(child.pid,signal.SIGTERM)
   except ProcessLookupError:pass
  try:state['wrapper_exit']=child.wait(timeout=5)
  except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);state['wrapper_exit']=child.wait(timeout=2)
 state['owned_exit_observations']=[]
 for _ in range(20):
  remaining={str(pid):now for pid,ident in owned.items()if(now:=identity(pid))and now['start']==ident['start']and now['state']not in ['Z','X']}
  state['owned_exit_observations'].append({'epoch':time.time(),'remaining':remaining})
  if not remaining:break
  time.sleep(.05)
 state['remaining_owned']=remaining
 if remaining:
  state['unexpected_survivors']=remaining;state['exit']=1
  for pid,expected in owned.items():
   now=identity(pid)
   if now and now['start']==expected['start']and now['state']not in ['Z','X']:
    try:os.kill(pid,signal.SIGKILL)
    except ProcessLookupError:pass
 state['input_drift']=[p for p,h in pins.items()if not Path(p).is_file()or sha(p)!=h];state['exit']=state['exit']or int(bool(state['input_drift']or state['remaining_owned']));state['pins']=pins;state['state']='terminal';state['graphics_lease']['released_epoch']=time.time();fcntl.flock(lease,fcntl.LOCK_UN);lease.close();(OUT/'status.json').write_text(json.dumps(state,indent=2)+'\n');print(json.dumps({k:state.get(k)for k in ['state','exit','display','actual_XTEST_key_received','error','input_drift','remaining_owned']}),flush=True)
raise SystemExit(state['exit'])
