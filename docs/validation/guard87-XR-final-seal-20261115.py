#!/usr/bin/env python3
"""Read-only validation and compact exact raw evidence for the final V6 matrix."""
import gzip, hashlib, io, json, os, pathlib, tarfile, time
ROOT=pathlib.Path('/home/blux/de-xr/dome-sculpt')
BASE=ROOT/'artifacts/guard87-xr-case-cache-v6-20261115'
DEST=ROOT/'artifacts/public-monitor-transaction-metadata-20261115/docs/validation'
CASES=['mtoon-quarter','mtoon-tiny','pbr-quarter','pbr-tiny']
SOURCES={'application_source':'3d1643827dd56e1b2ef1d57b8a9c6ff1e4211a2c','test_runtime_source':'c54f66f8feab6ff52331bb264b54fc55cdc14c01','XR_operational_scenario_source':'02d17f29dd63f4e379627489e9ec2d77f1147387'}
def sha(data): return hashlib.sha256(data).hexdigest()
def filehash(p):
 h=hashlib.sha256()
 with open(p,'rb') as f:
  for b in iter(lambda:f.read(1<<20),b''): h.update(b)
 return h.hexdigest()
def js(p): return json.loads(p.read_bytes())
receipt={'schema':1,'state':'terminal','exit':0,'scope':'Four coherent final-config real-loader synthetic XR cases, App3d/TestRuntimec54/coordinator02d. No physical headset, combined draft build, installation or full production-goal completion claim.','sources':SOURCES,'cases':[],'files':{},'current_pin_conflicts':[],'current_pin_drift':[],'total_checks':0,'preserved_failures':{'original_cache_disabled_MToon_quarter':'Actual240-second timeout after174 passing checks remains failure; firstcase receipt retains raw failure/timing.','original_test_runtime_3d':'Actual null JSON reporter failure after41 checks; no original3d-runtime pass claim.','original_outer_owning_build_tool12734':'Actual outer143 remains unknown; runtime admission rests on independently audited closed artifacts, never an outer invocation0 claim.'}}
pins={}; cache_roots=[]; allpids=[]
for case in CASES:
 p=BASE/f'release-xr-{case}-case-code-cache-v6'; s=js(p/'status.json'); r=js(p/'workflow/report.json'); c=js(p/'workflow/shader-code-cache-provenance.json')
 assert s['state']=='terminal' and s['exit']==s['process_exit']==0 and s['external_guard'] is None
 assert s['checks']==r['count']==194 and r['complete'] and r['error'] is None
 assert not s['input_drift'] and not s['remaining_owned_processes'] and not r['drift']
 assert s['lease_released_epoch']>s['lease_acquired_epoch'] and s['lease_before_any_graphics_child']
 for k,v in SOURCES.items(): assert s[k]==v,(case,k,s[k])
 assert s['original_limits']['XR_seconds']==240 and s['original_limits']['XR_physical_frames']==6000
 assert s['external_limits']=={'seconds':240,'descendant_RSS_bytes':4294967296}
 assert len(r['checks'])==194 and all(x['passed'] for x in r['checks'])
 ev=c['events']; ids=[e['actual_app_pid'] for e in ev]
 assert c['initially_empty'] and len(ev)==len(set(ids))==19
 assert ev[0]['disk_hits']==0 and ev[0]['disk_misses']>0 and ev[0]['cache_files']
 assert sum(e['disk_hits'] for e in ev[1:])>0
 assert set(ids)==set(s['actual_app_pids'])
 cache_roots.append(c['root']);allpids+=ids
 for path,digest in s['pins'].items():
  assert path not in pins or pins[path]==digest,('conflicting pin',path)
  pins[path]=digest
 paths=sorted(x for x in p.rglob('*') if x.is_file() and (x.suffix in {'.json','.jsonl','.maps','.png','.ppm','.txt','.log'}))
 archive=DEST/f'guard87-XR-final-{case}-20261115.tar.gz'
 member_manifest=[]
 with archive.open('wb') as out:
  with gzip.GzipFile(fileobj=out,mode='wb',mtime=0,filename='') as z:
   with tarfile.open(fileobj=z,mode='w|') as tar:
    for x in paths:
     raw=x.read_bytes(); rel=x.relative_to(p).as_posix(); ti=tarfile.TarInfo(rel);ti.size=len(raw);ti.mode=0o644;ti.mtime=0
     tar.addfile(ti,io.BytesIO(raw));member_manifest.append({'name':rel,'bytes':len(raw),'sha256':sha(raw)})
 receipt['files'][archive.name]={'sha256':filehash(archive),'bytes':archive.stat().st_size,'members':member_manifest}
 receipt['cases'].append({'case':case,'phase_exit':0,'process_exit':0,'checks':194,'seconds':s['seconds'],'status_sha256':filehash(p/'status.json'),'report_sha256':filehash(p/'workflow/report.json'),'cache_provenance_sha256':filehash(p/'workflow/shader-code-cache-provenance.json'),'owned_survivors':{},'input_drift':[],'lease_released_epoch':s['lease_released_epoch'],'cache_root':c['root'],'initially_empty':True,'first_App':{'pid':ev[0]['actual_app_pid'],'disk_hits':ev[0]['disk_hits'],'disk_misses':ev[0]['disk_misses'],'created_files':len(ev[0]['cache_files'])},'later_fresh_App_count':18,'later_disk_hits':sum(e['disk_hits'] for e in ev[1:]),'unique_App_PIDs':ids})
 receipt['total_checks']+=194
assert len(set(cache_roots))==4 and len(set(allpids))==76
for p,digest in pins.items():
 actual=filehash(p)
 if actual!=digest: receipt['current_pin_drift'].append({'path':p,'expected':digest,'actual':actual})
assert not receipt['current_pin_drift']
receipt['current_pin_count']=len(pins);receipt['current_pin_audit_epoch']=time.time()
receipt['same_operational_configuration']={'scenario_sha256':'d80456f6113dc4a2456dcc8c22e145ccc2e0f931d8933abf481b34f8ac63932e','runner_sha256':'6a46e254db011a33970c7cef6735d7bfe2b21f6f218b6108437ef3820a286340','test_runtime_module_sha256':'58d18b30966f08a5d2351511782a2e41956582a24a65111e944f65261c346665','four_distinct_initially_empty_cache_roots':True,'all_App_PIDs_unique':True,'fresh_per_App_scene_config_state':True,'cache_scope':'Mesa shader code only, within one case; no scene/image/native-state reuse','unchanged_limits':{'seconds':240,'physical_frames':6000,'correct_RGB_maximum':3,'geometry_minimum':12,'wrong_normal_minimum':7},'original_check_calls_AST_exact':29}
# Seal exact current-pin audit without expanding all immutable build/source paths into the concise receipt.
raw=json.dumps(pins,indent=2).encode()+b'\n'; name='guard87-XR-final-current-pins-20261115.json.gz';data=gzip.compress(raw,mtime=0);(DEST/name).write_bytes(data);receipt['files'][name]={'sha256':sha(data),'raw_sha256':sha(raw),'count':len(pins)}
out=DEST/'guard87-XR-final-fourcase-20261115.json';out.write_text(json.dumps(receipt,indent=2)+'\n'); print(json.dumps({'receipt':str(out),'sha256':filehash(out),'checks':receipt['total_checks'],'pins':len(pins),'cases':[{k:v for k,v in x.items() if k in ['case','checks','seconds','status_sha256','later_disk_hits']} for x in receipt['cases']]},indent=2))
