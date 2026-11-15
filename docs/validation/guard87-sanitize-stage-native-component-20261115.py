"""Build separate component archives from completed whole current compiler objects.
This is not the live owning archive action or overall application acceptance.
"""
from pathlib import Path
import json,subprocess,time,stat,os
from ninja_audit import sha,archive_members
from dependency_audit_v5 import audit
P=Path(__file__).resolve().parent;A=P.parents[1]/'artifacts/guard87-final-owning-sanitize-plan-20261115/sanitize-3d1643-current-fresh-v5-audit';E=A/'execution-v1';F=A/'plan.json'
assert sha(F)=='a02ea7b2d888728483e90b8aa1ccce28494b98d9cc7b8d90107a70497f2ddf4a'
j=json.loads(F.read_text());source=Path(j['source_path']);dest=Path(j['destination']);inputs=json.loads((E/'inputs.json').read_text())
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==j['source']
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source,text=True)
def no_drift():
 bad=[k for k,v in inputs.items()if not Path(k).is_file()or sha(k)!=v];assert not bad,('Current original compiler/header/source/SDK input drift',bad[:5])
no_drift()
journal=(E/'steps.jsonl').read_text();steps=[json.loads(x)for x in journal[:journal.rfind('\n')+1].splitlines()]
done={r['key']:r for r in steps if r['event']=='complete'and r['exit']==0}
archives={k:r for k,r in j['links'].items()if r['archive']and k!='libdome-sculpt-xr.a'};assert len(archives)==13
objects=set(k for r in archives.values()for k in r['object_inputs']);assert len(objects)==214 and objects<=set(done)
assert all(done[k]['kind']=='compile'and j['objects'][k]['route']=='compile-complete-TU'for k in objects)
for k in objects:
 assert len([x for x in steps if x['key']==k and x['event']=='launch'])==1
 assert len([x for x in steps if x['key']==k and x['event']=='complete'])==1
out=P/'component-from-current-whole-objects-v1';out.mkdir();stage=out/'stage';stage.mkdir()
subset=dict(j,objects={k:j['objects'][k]for k in sorted(objects)})
dependency=audit(subset,dest,out,inputs)
providers={}
for k in sorted(objects):
 old=dest/k;new=stage/k;new.parent.mkdir(parents=True,exist_ok=True);before=sha(old)
 subprocess.run(['cp','--reflink=always','--preserve=timestamps',str(old),str(new)],check=True)
 assert old.stat().st_ino!=new.stat().st_ino and sha(new)==before and sha(old)==before
 new.chmod(0o444);dep=dest/j['objects'][k]['depfile'];newdep=stage/j['objects'][k]['depfile'];newdep.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run(['cp','--reflink=always','--preserve=timestamps',str(dep),str(newdep)],check=True);assert sha(newdep)==sha(dep);newdep.chmod(0o444)
 providers[k]={'provider':str(old),'snapshot':str(new),'sha256':before,'actual_compiler_completion':done[k],'actual_command':j['objects'][k]['command'],'depfile_snapshot':str(newdep),'depfile_sha256':sha(dep),'separate_readonly_inode':True}
archive_steps=[];completed=set(objects);pending=dict(archives);owned={}
env=dict(os.environ,LD_LIBRARY_PATH=str(P.parents[1]/'artifacts/sdk-20261111/toolkit-sanitize/lib')+':/opt/fltk-vr/bdf-runtime/lib:/usr/lib64')
for name in ['LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']:env.pop(name,None)
env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
while pending:
 ready=[k for k,r in pending.items()if set(r['dag_dependencies'])<=completed]
 assert ready,('Unclosed component archive dependencies',list(pending))
 for k in ready:
  r=pending.pop(k);command=r['command'];assert str(dest)not in command,('Archive command would touch live output',k)
  assert all(not Path(x).is_absolute()and '..'not in Path(x).parts for x in [k,*r['object_inputs']])
  assert not(stage/k).exists();started=time.time();log=out/(k+'.log')
  with log.open('w')as f:actual=subprocess.run(['/bin/bash','-c',command],cwd=stage,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
  assert actual.returncode==0,(k,actual.returncode)
  wanted=[{'name':Path(x).name,'bytes':(stage/x).stat().st_size,'sha256':providers[x]['sha256']}for x in r['object_inputs']]
  assert archive_members(stage/k)==wanted,('Independent archive member order/payload differs',k)
  (stage/k).chmod(0o444);h=sha(stage/k);owned[k]={'path':str(stage/k),'sha256':h,'members':wanted}
  archive_steps.append({'key':k,'command':command,'cwd':str(stage),'exit':actual.returncode,'seconds':time.time()-started,'log_sha256':sha(log),'sha256':h});completed.add(k)
no_drift();assert all(sha(v['provider'])==v['sha256']and sha(v['snapshot'])==v['sha256']for v in providers.values())
receipt={'state':'terminal','exit':0,'source':j['source'],'profile':'current strict sanitizer C and C++ native component','scope':'Independent component archive producer from214 genuine whole completed current-toolchain object providers;13 actual archive commands in separate stage. NOT live owning archive action, NOT zero-reuse full application or runtime acceptance. Original application build untouched.','plan':{'path':str(F),'sha256':sha(F)},'producer_script':{'path':str(Path(__file__).resolve()),'sha256':sha(__file__)},'current_inputs':{'path':str(E/'inputs.json'),'sha256':sha(E/'inputs.json'),'drift':[]},'actual_whole_object_providers':providers,'actual_compiler_dependencies':dependency,'archive_producers':archive_steps,'archives':owned,'current_original_build_status':json.loads((E/'status.json').read_text())['state'],'excluded_archive':'libdome-sculpt-xr.a: native component excludes XR; its owner not required or copied','strict_environment':{k:env.get(k)for k in ['LD_LIBRARY_PATH','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LD_PRELOAD']},'live_owning_archives_modified':False,'later_corroboration':'Compare these ordered archive bytes to the independently completing live owning producer when available; no live owning action is claimed here.','created_epoch':time.time()}
(out/'component-ready.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'state':'terminal','exit':0,'receipt':str(out/'component-ready.json'),'sha256':sha(out/'component-ready.json'),'whole_objects':len(objects),'archives':len(owned),'dependency':dependency},indent=2))
