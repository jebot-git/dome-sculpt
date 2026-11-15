"""Authorized one-link launch context. Captures selected variables/tools, no app/ldd."""
import hashlib,json,os,shutil,signal,subprocess,sys,time
from pathlib import Path
P=Path(__file__).resolve().parent;ROOT=P.parents[1];UNIT='ds-app1bc-component-link-v4-20261115.service';PLAN='7935daa0e064eb9656fa90a63de567742dbb430a21615ef4cdac1f068aeaff3c';REVIEW='e459ac20da4670cd282be99aff5eabfd55a643be0da0b9e8d17e20f86c018c89';PRODUCER='2f2a46484b925c7d4cd3038a5da3b46c3a6aefeb2462c636270fa99e150074d2'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(d):
 p=P/'actual-launch-context-v4.json';t=p.with_suffix('.writing');t.write_text(json.dumps(d,indent=2)+'\n');t.replace(p)
assert sha(P/'plan-v3.json')==PLAN and sha(P/'root-link-review-v3.json')==REVIEW and sha(P/'produce-link-v1.py')==PRODUCER
plan=json.loads((P/'plan-v3.json').read_text());old=json.loads(Path(plan['authority']['plan_path']).read_text());env=dict(os.environ)
for k in['LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']:assert not env.get(k)
selected={k:env.get(k)for k in['PATH','LD_LIBRARY_PATH','LIBRARY_PATH','GCC_EXEC_PREFIX','COMPILER_PATH','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','PKG_CONFIG_PATH','SOURCE_DATE_EPOCH','LANG','LC_ALL','BASH_ENV','ENV','LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']}
assert selected['LD_LIBRARY_PATH']==plan['environment']['LD_LIBRARY_PATH']
tools={}
for path,row in old['current_toolchain']['tools'].items():
 assert sha(path)==row['sha256']and str(Path(path).resolve())==row['resolved'];tools[path]={'resolved':row['resolved'],'sha256':row['sha256'],'roles':row['roles']}
for path in['/bin/bash','/usr/bin/readelf']:
 assert sha(path)==plan['pins'][path];tools[path]={'resolved':str(Path(path).resolve()),'sha256':sha(path)}
control=json.loads((P/'control-tool-attestation-v4.json').read_text())
for path,row in control.items():
 assert str(Path(path).resolve())==row['resolved']and sha(path)==row['sha256'];tools[path]=row
queries={}
for flag in['-print-prog-name='+x for x in['cc1plus','cc1','collect2','ld','as']]+['-print-file-name='+x for x in['libasan.so','libubsan.so','libstdc++.so','libgcc_s.so']]:
 value=subprocess.check_output(['/usr/bin/c++',flag],env=env,text=True).strip();path=Path(value)if Path(value).is_absolute()else Path(shutil.which(value,path=env.get('PATH'))or value)
 assert str(path)in old['current_toolchain']['tools']and sha(path)==old['current_toolchain']['tools'][str(path)]['sha256'];queries[flag]={'reported':value,'actual_path':str(path),'sha256':sha(path)}
properties=subprocess.check_output(['/usr/bin/systemctl','--user','show',UNIT,'--property=MainPID,ExecMainPID,InvocationID,Type,MemoryMax,RuntimeMaxUSec,TimeoutStopUSec,KillMode,RemainAfterExit'],text=True);properties=dict(x.split('=',1)for x in properties.splitlines());assert properties['MainPID']==str(os.getpid())and properties['MemoryMax']==str(4*2**30)and properties['Type']=='exec'
state={'state':'launch-context-verified','exit':1,'unit':UNIT,'wrapper_pid':os.getpid(),'unit_properties':properties,'selected_actual_environment':selected,'actual_tools':tools,'actual_compiler_queries':queries,'plan_sha256':PLAN,'review_sha256':REVIEW,'producer_sha256':PRODUCER,'wrapper_sha256':sha(__file__),'project_app_or_ldd_executed':False};save(state)
command=['/usr/bin/python3','-u',str(P/'produce-link-v1.py'),str(P/'plan-v3.json'),'--plan-sha',PLAN,'--execute','--review',str(P/'root-link-review-v3.json'),'--review-sha',REVIEW]
log=P/'producer-service-v4.log'
with log.open('x')as f:
 child=subprocess.Popen(command,cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT);state.update(state='producer-running',producer_pid=child.pid,producer_command=command);save(state);code=child.wait()
state.update(state='terminal',exit=code,producer_actual_exit=code,log_sha256=sha(log));assert sha(P/'plan-v3.json')==PLAN and sha(P/'root-link-review-v3.json')==REVIEW and sha(P/'produce-link-v1.py')==PRODUCER;save(state)
raise SystemExit(code)
