"""One fresh App1bc component link only, after root plan review. No app/ldd run."""
import argparse,json,os,shutil,signal,stat,sys,time
from pathlib import Path
from component_link_common import sha,identity,ld_dependencies,dynamic_metadata
from ninja_audit import archive_members
from owned_monitor_v2 import run
P=Path(__file__).resolve().parent;received={'signal':None}
def term(sig,frame):received['signal']=sig;raise SystemExit(128+sig)
def write(p,d):
 t=p.with_suffix('.writing');t.write_text(json.dumps(d,indent=2)+'\n');t.replace(p)
def execute(plan_file,expected_plan_sha,review_file,review_sha):
 assert sha(plan_file)==expected_plan_sha and sha(review_file)==review_sha;plan=json.loads(plan_file.read_bytes());review=json.loads(review_file.read_bytes())
 assert review['state']=='reviewed-for-component-link-only'and review['plan_sha256']==expected_plan_sha and review['producer_sha256']==sha(__file__)
 assert review['no_app_or_product_ldd_execution']is True and plan['state']=='prepared-NOT-linked'and plan['scope_tuple']=={'application_product':'1bc13125','unchanged_component_context':'3d164382','literal_asset_context':'3d164382'}
 assert plan['command']==plan['original_actual_link_command']and len(plan['objects'])==27 and len(plan['archives'])==14
 out=P/'execution-v1';out.mkdir();trial=Path(plan['destination']);assert not trial.exists();trial.mkdir()
 state={'state':'preparing','exit':1,'scope':plan['scope'],'scope_tuple':plan['scope_tuple'],'plan':{'path':str(plan_file),'sha256':expected_plan_sha},'root_review':{'path':str(review_file),'sha256':review_sha},'copies':{},'link':None,'external_signal':None,'input_drift':None,'runtime_loader_admitted':False,'no_app_or_product_ldd_execution':True};start=time.monotonic()
 def save():write(out/'status.json',state)
 def drift():return[p for p,h in plan['pins'].items()if not Path(p).is_file()or sha(p)!=h]
 def copy(key,row):
  src=Path(row['provider']);dst=trial/key;dst.parent.mkdir(parents=True,exist_ok=True);assert not dst.exists()and not dst.is_symlink();before=identity(src);assert sha(src)==row['sha256']
  with src.open('rb')as a,dst.open('xb')as b:shutil.copyfileobj(a,b,1<<20)
  dst.chmod(0o444);after=identity(src);current=identity(dst)
  assert before==after and sha(dst)==sha(src)==row['sha256']and(current['dev'],current['ino'])!=(before['dev'],before['ino'])and dst.stat().st_nlink==1
  assert not(current['mode']&0o222)
  state['copies'][key]={'provider':str(src),'sha256':row['sha256'],'source_before':before,'source_after':after,'destination':str(dst),'destination_identity':current,'independent_inode':True,'readonly':True};save()
 env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
 for key in['LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']:env.pop(key,None)
 env.update(plan['environment']);env.update(LP_NUM_THREADS='2',OMP_NUM_THREADS='2',OPENBLAS_NUM_THREADS='2')
 try:
  save();assert not drift(),'Current pinned input drift before staging'
  for key,row in list(plan['objects'].items())+list(plan['archives'].items()):copy(key,row)
  for key,row in plan['archives'].items():assert archive_members(trial/key)==row['members']
  assert not(trial/'dome-sculpt').exists()
  state['state']='linking';save()
  def progress(row):state['link']=row;save()
  step=run(['/bin/bash','-c',plan['command']],cwd=trial,env=env,log=out/'link.log',timeout=900,max_rss=4*2**30,progress=progress);state['link']=step;save()
  if received['signal']:raise SystemExit(128+received['signal'])
  assert step.get('exit')==0 and not step.get('error'),('Fresh complete application link failed',step)
  product=trial/'dome-sculpt';assert product.is_file()and product.stat().st_size>0
  actual=ld_dependencies(trial/'CMakeFiles/dome-sculpt.dir/link.d');assert actual['target']=='dome-sculpt'
  assert actual['names']==plan['link_dependency_forecast']['literal_names'],('Actual linker input roster/order differs from presealed forecast',actual)
  actual_deps=[]
  for name in actual['names']:
   p=Path(name);p=p if p.is_absolute()else trial/p;physical=str(p.resolve());expected=plan['link_dependency_forecast']['expected_physical'][physical];assert sha(p)==expected
   actual_deps.append({'literal':name,'path':str(p),'resolved':physical,'sha256':expected})
  write(out/'actual-link-dependencies.json',{'scope':'Actual fresh GNU linker dependency file, not a forecast or cached product','link_exit':step['exit'],'raw_record':actual,'records':actual_deps})
  metadata=dynamic_metadata(product,env);original=plan['old_application_dynamic_metadata'];assert all(metadata[k]==original[k]for k in['NEEDED','RUNPATH','RPATH'])
  write(out/'new-ELF-dynamic-metadata.json',metadata)
  for key,row in state['copies'].items():
   assert identity(row['provider'])==row['source_before']==row['source_after']and sha(row['provider'])==sha(trial/key)==row['sha256']
   assert identity(trial/key)==row['destination_identity']
  state['input_drift']=drift();assert not state['input_drift']
  state.update(exit=0,product={'path':str(product),'sha256':sha(product),'bytes':product.stat().st_size},actual_link_dependencies={'path':str(out/'actual-link-dependencies.json'),'sha256':sha(out/'actual-link-dependencies.json'),'count':len(actual_deps)},ELF_metadata={'path':str(out/'new-ELF-dynamic-metadata.json'),'sha256':sha(out/'new-ELF-dynamic-metadata.json'),'same_NEEDED_RUNPATH_RPATH_as_old':True},component_trial_only=True)
 except BaseException as error:
  state.update(exit=128+received['signal']if received['signal']else 1,error=repr(error),external_signal=received['signal'])
 finally:
  state.update(state='terminal',seconds=time.monotonic()-start);save()
 print(json.dumps({k:state.get(k)for k in['state','exit','product','external_signal','error','input_drift']}),flush=True);return state['exit']
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('plan',type=Path);p.add_argument('--plan-sha');p.add_argument('--execute',action='store_true');p.add_argument('--review',type=Path);p.add_argument('--review-sha');a=p.parse_args();signal.signal(signal.SIGTERM,term);signal.signal(signal.SIGINT,term)
 if a.execute:
  assert a.plan_sha and a.review and a.review_sha;raise SystemExit(execute(a.plan.resolve(),a.plan_sha,a.review.resolve(),a.review_sha))
 d=json.loads(a.plan.read_text());print(json.dumps({'state':'inspection-only-NOT-linked','scope':d['scope'],'source_tuple':d['scope_tuple'],'objects':len(d['objects']),'archives':len(d['archives']),'link_command':d['command']},indent=2))
