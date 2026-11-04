"""Read-only native widget observations; all interaction uses real X events."""
import asyncio,json,time
from Xlib import X,XK
from Xlib.ext import xtest
class BakeWidgets:
 def __init__(self,path,connection):self.path,self.connection=path,connection
 def find(self,label):
  try:windows=json.loads(self.path.read_text())
  except (OSError,ValueError):return None
  def walk(w):
   if w['label']==label:return w
   for child in w.get('children',[]):
    result=walk(child)
    if result:return result
  for window in windows:
   if window['label'].startswith('Bake surface'):
    result=walk(window)
    if result:return result
 async def item(self,label):
  end=time.monotonic()+30
  while time.monotonic()<end:
   value=self.find(label)
   if value:return value
   await asyncio.sleep(.04)
  raise TimeoutError('Actual native widget was not observed: '+label)
 def pointer(self,window,item,button):
  g=window.get_geometry();x=item['x']+min(12,item['w']/2);y=item['y']+item['h']/2
  xtest.fake_input(self.connection,X.MotionNotify,x=int(g.x+x),y=int(g.y+y));xtest.fake_input(self.connection,X.ButtonPress,button);xtest.fake_input(self.connection,X.ButtonRelease,button);self.connection.sync()
 async def visible(self,window,label):
  for _ in range(80):
   item=await self.item(label);g=window.get_geometry();footer=2*max(36,item['label_size']+16);viewport=g.height-footer
   if item['y']>=1 and item['y']+item['h']<=viewport-1:return item
   rect={'x':2,'y':max(4,viewport/2),'w':1,'h':1}
   self.pointer(window,rect,5 if item['y']+item['h']>viewport else 4);await asyncio.sleep(.08)
  raise TimeoutError('Actual native scroll did not expose '+label)
 async def click(self,window,label):self.pointer(window,await self.visible(window,label),1);await asyncio.sleep(.1)
 async def wheel(self,window,label,button=5):
  item=await self.visible(window,label);item={**item,'x':item['x']+item['w']*.6,'w':1};self.pointer(window,item,button);await asyncio.sleep(.1)
 async def focus(self,label):
  for _ in range(60):
   if (await self.item(label))['focused']:return
   code=self.connection.keysym_to_keycode(XK.string_to_keysym('Tab'));xtest.fake_input(self.connection,X.KeyPress,code);xtest.fake_input(self.connection,X.KeyRelease,code);self.connection.sync();await asyncio.sleep(.08)
  raise TimeoutError('Actual native keyboard focus did not reach '+label)
