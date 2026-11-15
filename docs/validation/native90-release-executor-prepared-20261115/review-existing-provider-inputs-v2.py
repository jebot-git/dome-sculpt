"""Read only existing genuine Release3d MD/journal files. No compiler invocation.
This proves parser/original-command preservation only, not a new mixed build pass.
"""
import hashlib,json,time
from pathlib import Path
from mixed_dependency_audit_v1 import parse_actual_md,actual_jobs
P=Path(__file__).resolve().parent;ROOT=P.parents[1];BASE=ROOT/'artifacts/current-toolchain-guard87-release-20261115'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
module_pins={str(x):sha(x)for x in[P/'mixed_dependency_audit_v1.py',P/'current_provider_routes_v2.py',Path(__file__).resolve()]}
pp=BASE/'release-3d1643-current-fresh-v2-audit/plan.json';assert sha(pp)=='0d03a29a1d2251d1186cb8a9c94f22227566c86664245c51b050a66e458b7dec';plan=json.loads(pp.read_bytes());build=Path(plan['destination'])
status=json.loads((pp.parent/'execution-v1/status.json').read_bytes());d=status['actual_fresh_compiler_dependencies'];assert sha(d['path'])==d['sha256']=='6aead1793cae61e46a9b3a9a2a366970b21ac86f5b970dedd425971daf11f5bc';deps=json.loads(Path(d['path']).read_bytes())
journal=Path(status['steps_journal']);launched,completed=actual_jobs(journal);assert len(launched)==len(completed)==1768
records={};incidences=0
for key,row in deps['records'].items():
 parsed=parse_actual_md(row['depfile'],build,key);assert parsed['raw_sha256']==row['depfile_sha256']and parsed['actual_target']==row['actual_target']
 assert[x['resolved']for x in parsed['dependencies']]==[x['resolved']for x in row['dependencies']]
 a=launched[key];b=completed[key];assert a['kind']==b['kind']=='compile'and a['pid']==b['pid']and b['exit']==0
 assert a['argv']==['nice','-n','10',*plan['objects'][key]['command']]and a['cwd']==str(build)
 incidences+=len(parsed['dependencies']);records[key]={'actual_original_depfile':row['depfile'],'raw_sha256':parsed['raw_sha256'],'actual_original_compile_cwd':a['cwd'],'actual_compile_argv_sha256':hashlib.sha256(json.dumps(a['argv']).encode()).hexdigest(),'actual_original_target':parsed['actual_target'],'dependency_names_preserved':len(parsed['dependencies']),'actual_original_completion_exit':b['exit']}
assert len(records)==1032 and incidences==433074
report={'state':'terminal','exit':0,'scope':'Read-only parser and original command/CWD contract checked against genuine existing closed3d inputs. No synthetic compiler output, dependency rewrite, new compilation, staging, linking, product execution or mixed-build acceptance.','original_provider_source':plan['source'],'original_outer143_preserved':True,'actual_original_journal':{'path':str(journal),'sha256':sha(journal),'launch_events':len(launched),'complete_events':len(completed)},'genuine_original_MD_owners':len(records),'actual_dependency_incidences':incidences,'records':records,'observed_epoch':time.time(),'parser_module_inputs':module_pins}
assert all(sha(x)==v for x,v in module_pins.items())
out=P/'existing-provider-parser-review-v2.json';out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items()if k!='records'},indent=2));print('SHA256',sha(out))
