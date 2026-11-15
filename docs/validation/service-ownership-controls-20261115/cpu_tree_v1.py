"""Owned CPU-only child/grandchild/sentinel controls; never a project product."""
from pathlib import Path
import argparse,os,signal,subprocess,time,json
P=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--role',choices=['leader','grandchild','sentinel'],required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--mode',choices=['healthy','orphan','term','deadline','rss'],default='healthy');a=p.parse_args();a.out.mkdir(exist_ok=True,parents=True)
def ident():
 raw=Path('/proc/self/stat').read_text();v=raw[raw.rfind(')')+2:].split();return {'pid':os.getpid(),'start':int(v[19]),'ppid':os.getppid(),'pgrp':os.getpgrp(),'session':os.getsid(0),'cgroup':Path('/proc/self/cgroup').read_text().strip()}
def write(name,value):
 f=a.out/name;t=f.with_suffix('.writing');t.write_text(json.dumps(value,indent=2)+'\n');t.replace(f)
started=time.monotonic();signal.signal(signal.SIGTERM,signal.SIG_IGN if a.mode in ['orphan','term','deadline','rss']and a.role!='sentinel'else signal.SIG_DFL)
resident=bytearray(48*2**20 if a.role=='grandchild'and a.mode=='rss'else 0)
for i in range(0,len(resident),4096):resident[i]=i%251
write(a.role+'.json',{**ident(),'allocated_touched_bytes':len(resident),'monotonic_start':started})
child=None
if a.role=='leader':
 child=subprocess.Popen(['/usr/bin/python3',str(P/'cpu_tree_v1.py'),'--role','grandchild','--out',str(a.out),'--mode',a.mode],start_new_session=True)
 until=time.monotonic()+3
 while not(a.out/'grandchild.json').exists()and time.monotonic()<until:time.sleep(.01)
 assert(a.out/'grandchild.json').exists()
 if a.mode=='orphan':raise SystemExit(0)
limit=90 if a.role=='sentinel'else .5 if a.mode=='healthy'else 12
counter=0;next_beat=started
while time.monotonic()-started<limit:
 for _ in range(5000):counter=(counter*1664525+1013904223)&0xffffffff
 if a.role=='sentinel':
  if time.monotonic()>=next_beat:write('sentinel-heartbeat.json',{**ident(),'counter':counter,'epoch':time.time()});next_beat=time.monotonic()+.05
  time.sleep(.005)
if child is not None:child.wait(timeout=2)
write(a.role+'-finished.json',{**ident(),'counter':counter,'seconds':time.monotonic()-started})
