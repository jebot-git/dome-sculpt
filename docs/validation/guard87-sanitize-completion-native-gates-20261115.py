"""Plan by default; execute only after combined-current final87 whole-owning DAG admission.
No GUI/OpenXR app/Arc is launched here. The manual render phase is software GL.
"""
import argparse,fcntl,hashlib,json,os,re,signal,subprocess,time
from pathlib import Path
P=Path(__file__).resolve().parent;R=P.parents[1];A=P/'sanitize-3d1643-completion-v1-audit';HEAD='3d1643827dd56e1b2ef1d57b8a9c6ff1e4211a2c'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def identity(pid):
 try:
  raw=(Path('/proc')/str(pid)/'stat').read_text();v=raw[raw.rfind(')')+2:].split();return {'start':int(v[19]),'pgrp':int(v[2])}
 except(FileNotFoundError,ProcessLookupError):return None
def main(args):
 plan=json.loads((A/'plan.json').read_text());assert plan['source']==HEAD;B=Path(plan['destination']);required=plan['selected_CTests'];assert len(required)==len(set(required))==87
 phase_plan={'source':HEAD,'phase':args.phase,'tag':args.tag,'build':str(B),'requires_complete_owning_status':str(A/'execution-v1/status.json'),'limits':{'selected_native_seconds':2400,'default_individual_CTest_seconds':180,'workspace27_seconds':300,'workspace373_seconds':300,'raw90159_seconds':300,'producer_seconds':900,'font_seconds':180,'manual_software_seconds':900},'requires_native87_for_fixtures_manual':True,'requires_common_graphics_lease':True,'provenance_scope':'Explicit combined current-owner completion:538 closed current providers+494 fresh complete TUs,733fresh links,3fresh generators; original143 remainsfailure, not freshsingleinvocation.','scope':'Exact current owning native/software gates only; no public GUI/XR, physical HMD or Arc/performance claim.'}
 if not args.execute:print(json.dumps(phase_plan,indent=2));return 0
 actual=json.loads((A/'execution-v1/status.json').read_text());assert actual['state']=='terminal'and actual['exit']==0 and not actual['input_drift']and actual['source']==HEAD;assert [actual[k]for k in ['completed_generators','completed_compilations','completed_links']]==[3,494,733]
 stage=actual['staged_whole_providers'];assert stage['count']==538 and sha(stage['path'])==stage['sha256'];staged=json.loads(Path(stage['path']).read_text());assert len(staged)==538 and all(plan['objects'][k]['route']=='reuse-authenticated-whole-object'for k in staged);assert plan['summary']['configured_TUs']==1032 and plan['summary']['fresh_complete_TUs']==494 and plan['summary']['reused_whole_objects']==538 and plan['summary']['retained_whole_products']==0
 dep=actual['actual_current_dependency_audit'];assert dep['owners']==1032 and dep['errors']==0 and sha(dep['path'])==dep['sha256'];dependency_manifest=json.loads(Path(dep['path']).read_text());assert dependency_manifest['state']=='passed'and len(dependency_manifest['records'])==1032
 routes=[v['current_owner_route']for v in dependency_manifest['records'].values()];assert routes.count('reuse-authenticated-whole-object')==538 and routes.count('compile-complete-TU')==494
 outputs=Path(actual['owned_outputs']['path']);assert sha(outputs)==actual['owned_outputs']['sha256'];owned=json.loads(outputs.read_text());assert len(owned)==1768;assert all(sha(B/k)==h for k,h in owned.items());pins=dict(plan['pins']);pins.update({str(B/k):h for k,h in owned.items()});pins.update({str(A/'execution-v1/status.json'):sha(A/'execution-v1/status.json'),str(outputs):sha(outputs),str(Path(__file__).resolve()):sha(__file__)});pins[dep['path']]=dep['sha256'];pins[stage['path']]=stage['sha256']
 for row in dependency_manifest['records'].values():pins[row['depfile']]=row['depfile_sha256']
 assert all(Path(k).is_file()and sha(k)==h for k,h in pins.items())
 out=P/(args.tag+'-'+args.phase);out.mkdir();state={**phase_plan,'state':'running','pid':os.getpid(),'steps':[],'exit':1};write(out/'inputs-before.json',pins)
 env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
 for k in ['ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LD_PRELOAD','DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','__EGL_VENDOR_LIBRARY_FILENAMES']:env.pop(k,None)
 env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',LD_LIBRARY_PATH=str(R/'artifacts/sdk-20261111/toolkit-sanitize/lib')+':/opt/fltk-vr/bdf-runtime/lib:/usr/lib64',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',OPENBLAS_NUM_THREADS='2',OMP_NUM_THREADS='2',EGL_PLATFORM='surfaceless',XR_RUNTIME_JSON=str(out/'no-physical-runtime.json'),CTEST_PARALLEL_LEVEL='1',PYTHONDONTWRITEBYTECODE='1')
 for key,leaf in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[key]=str(out/leaf);Path(env[key]).mkdir(mode=0o700)
 appearance=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';appearance.parent.mkdir();appearance.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':1}));assert env['HOME']==os.environ['HOME'];pins[str(appearance)]=sha(appearance)
 def save():write(out/'status.json',state)
 def step(name,command,timeout,expected=None):
  log=out/(name+'.log');row={'name':name,'command':command,'timeout_seconds':timeout,'started':time.time()};state['steps'].append(row);save()
  with log.open('w')as f:
   child=subprocess.Popen(command,cwd=B,env=env,stdout=f,stderr=subprocess.STDOUT,start_new_session=True);ident=identity(child.pid);row.update(pid=child.pid,identity=ident);state['phase']=name;save();print(name,'pid',child.pid,flush=True)
   try:exit=child.wait(timeout=timeout)
   except subprocess.TimeoutExpired:
    for sig in [signal.SIGTERM,signal.SIGKILL]:
     now=identity(child.pid)
     if now and now==ident:
      try:os.killpg(child.pid,sig)
      except ProcessLookupError:pass
     try:child.wait(timeout=3);break
     except subprocess.TimeoutExpired:pass
    exit=124;row['timeout']=True
  text=log.read_text();row.update(exit=exit,seconds=time.time()-row['started'],log_sha256=sha(log));save();print(name,'terminal',exit,flush=True);assert exit==0,(name,exit)
  if expected:assert expected in text,(name,'missing exact native count',expected)
  return text
 lease=None
 try:
  lease=Path('/tmp/dome-sculpt-arc-device.lock').open('a+');fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB);state['graphics_lease']={'path':lease.name,'acquired_epoch':time.time(),'owner_pid':os.getpid(),'stat':{'device':os.fstat(lease.fileno()).st_dev,'inode':os.fstat(lease.fileno()).st_ino},'before_any_built_product':True};save()
  if args.phase!='native':
   n=P/(args.native_tag+'-native/status.json');previous=json.loads(n.read_text());assert previous['state']=='terminal'and previous['exit']==0 and not previous['input_drift']and previous['source']==HEAD and previous['exact_native_passed']==sorted(required);pins[str(n)]=sha(n)
  if args.phase=='native':
   step('app-version',[str(B/'dome-sculpt'),'--version'],60)
   regex='^('+'|'.join(re.escape(n)for n in required)+')$';listing=subprocess.check_output(['ctest','--test-dir',str(B),'--show-only=json-v1','-R',regex],env=env,text=True);(out/'selected-tests.json').write_text(listing);selected=json.loads(listing)['tests'];assert sorted(x['name']for x in selected)==sorted(required);assert all(x.get('command')and Path(x['command'][0]).is_file()for x in selected);assert all(not any(p['name']=='DISABLED'and p['value']for p in x.get('properties',[]))for x in selected)
   text=step('native87',['ctest','--test-dir',str(B),'--output-on-failure','--timeout','180','--output-junit',str(out/'native87.xml'),'-R',regex],2400);passed=re.findall(r'Test\s+#\d+:\s+(\S+)\s+\.+\s+Passed',text);assert sorted(passed)==sorted(required)and len(passed)==87;assert '***Skipped'not in text and '***Not Run'not in text;state['exact_native_passed']=sorted(passed);save()
   step('workspace27',[str(B/'dome-sculpt-sculpt-chunk-workspace-test')],300,'PASS total 27');step('workspace373',[str(B/'dome-sculpt-sculpt-placed-workspace-test')],300,'PASS 373 placed-owner workspace controls');step('raw90159',[str(B/'dome-sculpt-baked-emission-workflow-test'),'--raw-source-worker-controls',str(out/'raw-uv1-worker')],300,'Baked emission raw UV1 worker 90159 checks PASS')
  elif args.phase=='fixtures':
   binary=str(B/'dome-sculpt-baked-emission-workflow-test');step('source-worker',[binary,'--source-worker-controls',str(out/'source-worker')],900,'180373 checks PASS');step('source-controls',[binary,'--source-controls'],60,'Baked emission source 27 checks PASS');step('original-worker',[binary,str(out/'original-worker')],900,'Baked emission worker 16417 checks PASS');step('original-public-producer',[binary,'--public-fixtures',str(out/'original-fixtures')],900,'Baked emission public producer 32794 checks PASS');step('source8-public-producer',[str(B/'dome-sculpt-source8-public-fixture'),'--public-fixtures',str(out/'source8-fixtures')],900)
   suite=json.loads((out/'source8-fixtures/manifest.json').read_text());assert suite['version']==2 and set(suite['cases'])=={'pbr-quarter','pbr-tiny','mtoon-quarter','mtoon-tiny'};state['source8_manifests']=suite;step('font-reference',[str(B/'dome-sculpt-baked-emission-font-reference'),str(out/'titles.json')],180);title=json.loads((out/'titles.json').read_text());font=Path(title['font_file']);assert font.is_file()and title['font_pixels']==16 and set(title['masks'])=={'Dome Sculpt','Objects','Mesh tools','Mesh baking'};pins[str(font)]=sha(font);state['font_source_manifest']=title['font_manifest']
   state['generated_fixture_files']={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in ['.dsculpt','.json']and p.name not in ['status.json','inputs-before.json']}
  else:
   text=step('manual-source-render-software',[str(B/'dome-sculpt-texture-set-source-render-test'),str(out/'source-render')],900);state['manual_gate']='Explicit same-object Source39/40 independent sourceful pixel oracle; extra manual gate, not an additional selected CTest.'
  state['exit']=0
 except BaseException as e:state.update(exit=1,error=repr(e))
 post={p:sha(p)for p in pins};write(out/'inputs-after.json',post);state.update(state='terminal',input_drift=[p for p,h in pins.items()if post[p]!=h]);state['exit']=state['exit']or int(bool(state['input_drift']));
 if lease is not None:
  state.setdefault('graphics_lease',{})['released_epoch']=time.time();fcntl.flock(lease,fcntl.LOCK_UN);lease.close()
 save();print('TERMINAL',state['exit'],state.get('error'),flush=True);return state['exit']
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--phase',choices=['native','fixtures','manual-software'],required=True);parser.add_argument('--tag',required=True);parser.add_argument('--native-tag',default='sanitize-completion-v1');parser.add_argument('--execute',action='store_true');raise SystemExit(main(parser.parse_args()))
