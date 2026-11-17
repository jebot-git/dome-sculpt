"""One reviewed software GL component matrix; no Arc, UI, or physical XR."""
import fcntl,hashlib,importlib.util,json,os,re,sys,threading
from pathlib import Path
P=Path(__file__).resolve().parent

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write(p,j):Path(p).write_text(json.dumps(j,indent=2)+'\n')
assert len(sys.argv)==2,'Pass exact root-reviewed software plan SHA256'
assert sha(P/'software-plan-v3.json')==sys.argv[1]
J=json.loads((P/'software-plan-v3.json').read_text());pins=dict(J['input_pins']);out=Path(J['output']);out.mkdir(exist_ok=False)
state={'state':'preflight','exit':1,'source':J['source'],'phase':'software','plan_sha256':sys.argv[1],'scope':J['scope'],'steps':[]};lease=None;stop=threading.Event();thread=None

def save():write(out/'status.json',state)
def drift():return[p for p,h in pins.items()if not Path(p).is_file()or sha(p)!=h]
try:
 assert sha(__file__)==J['driver_sha256'];assert not drift(),('Input drift',drift())
 M=Path(J['monitor']);spec=importlib.util.spec_from_file_location('qualified_software_monitor',M);monitor=importlib.util.module_from_spec(spec);spec.loader.exec_module(monitor)
 H=Path(J['nonELF_buffer_helper']);assert str(H)in pins and sha(H)==pins[str(H)]
 spec=importlib.util.spec_from_file_location('exact_live_shared_buffer',H);buffers=importlib.util.module_from_spec(spec);spec.loader.exec_module(buffers)
 linked=json.loads(Path(J['link_status']).read_text());assert linked['state']=='terminal'and linked['exit']==0 and not linked['input_drift']and linked['static_ELF_contract_same']and not any(linked['link_roster_difference'].values())
 lease=Path(J['common_lease']).open('a+');fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB);state['lease']={'owner':monitor.identity(os.getpid()),'released':False}
 env=dict(J['environment'])
 for key,leaf in [('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime')]:
  env[key]=str(out/leaf);Path(env[key]).mkdir(mode=0o700)
 env['XR_RUNTIME_JSON']=str(out/'no-physical-runtime.json');state['environment']=env
 assert not any(k in env for k in ['LD_PRELOAD','DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','DBUS_SESSION_BUS_ADDRESS'])
 write(out/'inputs-before.json',pins)
 def run(name,argv,collect=False):
  fast={'rows':set(),'fds':set(),'samples':0,'nonELF_buffers':{},'buffer_certification_errors':{}};stop.clear();thread_holder=[]
  def progress(row):
   state['live']={'name':name,**row};save()
   if not collect or thread_holder:return
   root={'pid':row['pid'],'start':row['start'],'pgrp':row['pgrp'],'session':row['root']['session']}
   def sample():
    while not stop.is_set():
     now=monitor.identity(root['pid'])
     if not now or any(now[k]!=v for k,v in root.items()):return
     try:
      rows=Path('/proc',str(root['pid']),'maps').read_text().splitlines();fast['rows'].update(rows);fast['samples']+=1
      for value in rows:
       if buffers.eligible(value)and value not in fast['nonELF_buffers']:
        try:fast['nonELF_buffers'][value]=buffers.certify(root,value)
        except(AssertionError,OSError)as error:fast['buffer_certification_errors'].setdefault(value,repr(error))
      for f in Path('/proc',str(root['pid']),'fd').iterdir():
       try:
        value=os.readlink(f)
        if value.startswith('/dev/dri/'):fast['fds'].add(value)
       except OSError:pass
     except OSError:pass
     stop.wait(.001)
   thread=threading.Thread(target=sample,daemon=True);thread_holder.append(thread);thread.start()
  row=monitor.run(argv,cwd=out,env=env,log=out/(name+'.log'),timeout=900,max_rss=4*1024**3,progress=progress)
  stop.set()
  if thread_holder:thread_holder[0].join(timeout=3);assert not thread_holder[0].is_alive()
  row.update(name=name,log_sha256=sha(out/(name+'.log')));state['steps'].append(row);state.pop('live',None)
  write(out/(name+'-maps.json'),{'rows':sorted(fast['rows']),'samples':fast['samples'],'live_DRM_FDs':sorted(fast['fds']),'certified_nonELF_buffers':fast['nonELF_buffers'],'buffer_certification_errors':fast['buffer_certification_errors']})
  mapped={}
  for value in fast['rows']:
   fields=value.split(None,5)
   if len(fields)!=6 or not fields[5].startswith('/'):continue
   if fields[5].endswith('(deleted)'):
    assert value in fast['nonELF_buffers'],('Unproven deleted mapping',value)
    continue
   path=Path(fields[5])
   try:
    with path.open('rb')as f:magic=f.read(4)
   except OSError as error:raise AssertionError(('Cannot classify file-backed mapping',str(path),repr(error)))
   if magic!=b'\x7fELF':continue
   expected=pins.get(str(path),pins.get(str(path.resolve())));assert expected is not None,('Unbound mapped ELF',str(path));assert sha(path)==expected,('Mapped ELF drift',str(path));mapped[str(path)]=expected
  row.update(mapped_ELFs=mapped,certified_nonELF_buffers=fast['nonELF_buffers'],live_DRM_FDs=sorted(fast['fds']),live_samples=fast['samples']);save()
  assert row.get('exit')==0 and not row.get('error')and not row['signals'],(name,row.get('exit'),row.get('error'))
  assert not [v for v in row['known_processes']if monitor.alive(v)]
  return(out/(name+'.log')).read_text(),fast
 text,_=run('ldd',['/usr/bin/ldd',J['product']['path']]);assert 'not found'not in text
 providers=[]
 for literal in re.findall(r'(/[^\s()]+)',text):
  if not Path(literal).is_file():continue
  resolved=str(Path(literal).resolve());expected=pins.get(literal,pins.get(resolved));assert expected is not None and sha(literal)==expected,('Unbound loader provider',literal,resolved);providers.append({'literal':literal,'resolved':resolved,'sha256':expected})
 state['loader_providers']=providers;save()
 images=out/'images';images.mkdir();text,fast=run('advanced-render',[J['product']['path'],'--advanced-display-render',str(images),J['witness_manifest']['path']],True)
 assert re.search(r'^ACTUAL_RENDERER llvmpipe',text,re.M),'Actual software renderer was not llvmpipe'
 assert len(re.findall(r'^ADVANCED_GL_FAMILY ',text,re.M))==17
 assert len(re.findall(r'^ADVANCED_GL_CASE ',text,re.M))==68
 assert len(re.findall(r'^ADVANCED_GL_POINT ',text,re.M))==J['expected_points']
 assert re.search(r'^PASS [1-9][0-9]* advanced display source/second-Bake graphical controls families=17 views=68 points='+str(J['expected_points'])+r'$',text,re.M)
 assert fast['samples']>0 and not fast['fds'];mapped=state['steps'][-1]['mapped_ELFs']
 for p in J['required_mapped_ELFs']:assert p in mapped,('Required software/canonical provider not mapped',p)
 files=sorted(images.glob('*.png'));assert len(files)==68+4*J['expected_points'],('Expected oneactual+4controls per original/extra probe',len(files))
 state['image_manifest']={str(p.relative_to(out)):sha(p)for p in files};state['summary']={'families':17,'views':68,'probes':J['expected_points'],'original_probes':272,'images':len(files),'software_renderer':'llvmpipe','RGB_bound':3,'negative_gap':7,'normal_bound':.035,'negative_modes':['wrong-domain','omitted','swapped-role'],'source_and_second_Bake':True};state['exit']=0
except BaseException as error:state['error']=repr(error)
finally:
 stop.set();state['input_drift']=drift();state['exit']=state['exit']or int(bool(state['input_drift']));write(out/'inputs-final.json',pins)
 state['retained_images']={str(p.relative_to(out)):sha(p)for p in sorted((out/'images').glob('*.png'))}if(out/'images').is_dir()else{}
 if lease:
  if 'lease'in state:state['lease']['released']=True
  fcntl.flock(lease,fcntl.LOCK_UN);lease.close()
 state['state']='terminal';save();print('TERMINAL',state['exit'],state.get('error'),flush=True)
raise SystemExit(state['exit'])
