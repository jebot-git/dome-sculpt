#!/usr/bin/env python3
"""Drive native FLTK/GL on a private display; no user document or session is used."""
import json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from Xlib import X,XK,display
from Xlib.ext import xtest
from PIL import Image,ImageChops
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_SCULPT_TEST_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_SCULPT_TEST_BUS':'1'})
root=Path(__file__).resolve().parents[1];out=root/'artifacts/gui';out.mkdir(parents=True,exist_ok=True)
processes=[];checks=[]
def check(value,label):
 checks.append({'passed':bool(value),'check':label});print(('PASS 'if value else'FAIL ')+label,flush=True)
 (out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2)+'\n');assert value,label
try:
 with tempfile.TemporaryDirectory(prefix='dome-sculpt-gui-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1280x900x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
  with os.fdopen(rd) as stream:screen=':'+stream.readline().strip()
  env={**os.environ,'HOME':home,'XDG_RUNTIME_DIR':home,'XDG_CONFIG_HOME':home+'/config','XDG_DATA_HOME':home+'/data','DISPLAY':screen,'FLTK_BACKEND':'x11','LIBGL_ALWAYS_SOFTWARE':'1','XDG_CURRENT_DESKTOP':'Dome','XMODIFIERS':'@im=none'}
  for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
  log=(out/'app.log').open('w');app=subprocess.Popen([str(root/'build/dome-sculpt')],env=env,stdout=log,stderr=log);processes.append(app)
  d=display.Display(screen)
  until=time.monotonic()+30;window=None
  while time.monotonic()<until:
   assert app.poll() is None,(out/'app.log').read_text()
   window=next((w for w in d.screen().root.query_tree().children if 'Dome Sculpt' in str(w.get_wm_name()or'')),None)
   if window:break
   time.sleep(.05)
  assert window,'No native sculpt window appeared'
  window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();time.sleep(.5)
  def motion(x,y):
   g=window.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+x,y=g.y+y);d.sync();time.sleep(.2)
  def key(name,down=None):
   kinds=[X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]
   for kind in kinds:xtest.fake_input(d,kind,d.keysym_to_keycode(XK.string_to_keysym(name)));d.sync();time.sleep(.05)
  def button(down):xtest.fake_input(d,X.ButtonPress if down else X.ButtonRelease,1);d.sync();time.sleep(.2)
  def shot(name):
   time.sleep(.25);g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);image=Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX');image.save(out/(name+'.png'));return image.crop((80,100,g.width-80,g.height-80))
  before=shot('01-native-sculpt');check(before.getcolors(before.width*before.height) is not None and len(before.getcolors(before.width*before.height))>100,'FLTK/OpenGL renders a shaded native sculpt surface')
  key('2');motion(550,365);key('Shift_L',True);button(True)
  for x in (565,580,595):motion(x,365)
  button(False);key('Shift_L',False);carved=shot('02-carved')
  check(ImageChops.difference(before,carved).getbbox() is not None,'Shift and trackball drag applies the selected carving tool')
  key('Control_L',True);key('z');key('Control_L',False);restored=shot('03-undo')
  check(ImageChops.difference(before,restored).getbbox() is None,'Native Ctrl+Z restores the pre-stroke surface exactly')
  motion(550,400);button(True);motion(650,460);button(False);orbited=shot('04-trackball')
  check(ImageChops.difference(restored,orbited).getbbox() is not None,'Default left drag uses trackball navigation')
  key('f');reset=shot('05-view-reset');check(ImageChops.difference(restored,reset).getbbox() is None,'Fit/reset restores the trackball view without changing the volume')
  d.close()
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
