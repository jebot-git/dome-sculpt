"""Read-only refinement: exact three historical documentary deltas are not code inputs.
No historical manifest rewrite, staging, configure, compiler, link or product run.
"""
import hashlib,json
from pathlib import Path
from component_link_common import sha,compiler_dependencies
P=Path(__file__).resolve().parent;ROOT=P.parents[1];base=P/'plan-v2.json';assert sha(base)=='8a64e5661abe4b9cd505033637ec1df208a34adec8eb9fff378b61fbc568319e';plan=json.loads(base.read_bytes());diagnostic=json.loads((P/'legacy-compile-input-drift-v1.json').read_text())
oldplan=json.loads(Path(plan['authority']['plan_path']).read_bytes());expected={x['path']:x for row in diagnostic['records']for x in row['drift']};assert len(expected)==3
roles={};all_commands=[];tool_paths=set(oldplan['current_toolchain']['tools']);rosters=[]
for folder in['artist-prepared-proof-v8-20261115','artist-prepared-proof-v9-20261115']:
 p=ROOT/'artifacts'/folder;cp=json.loads((p/'compile-plan.json').read_bytes());st=json.loads((p/'compile-status.json').read_bytes());assert st['state']=='terminal'and st['exit']==0 and not st['input_drift']
 for index,row in enumerate(cp['commands']):
  command=row['command'];actual=next(x for x in st['steps']if x['name']=='compile-'+str(index));pre=next(x for x in st['steps']if x['name']=='preprocess-'+str(index));assert actual['exit']==pre['exit']==0 and actual['command']==command
  dep=Path(command[command.index('-MF')+1]);full=p/(str(index)+'.preprocess.d')
  for path,kind in[(dep,'genuine compiler user-MMD'),(full,'separate genuine complete-M preprocess')]:
   parsed=compiler_dependencies(path);rosters.append({'path':str(path),'sha256':sha(path),'kind':kind,'dependencies':parsed['absolute_names']})
  all_commands.extend([actual['command'],pre['command']]);tool_paths.update([actual['command'][0],pre['command'][0],str(p/('compile-v8.py'if'v8'in folder else'compile-v9.py')),str(p/'compile-plan.json')])
for row in oldplan['generated_outputs'].values():
 all_commands.append([row['command']]);tool_paths.update(x['current']for x in row['inputs'])
all_commands.append([plan['command']]);tool_paths.update(x['provider']for x in list(plan['objects'].values())+list(plan['archives'].values()))
assert len(rosters)==12
for path,d in sorted(expected.items()):
 q=Path(path);assert sha(q)==d['current_actual']and '/docs/'in path and q.suffix in{'.md','.json'}
 assert all(str(q.resolve())not in{str(Path(x).resolve())for x in r['dependencies']}for r in rosters)
 assert path not in tool_paths
 assert all(path not in token for command in all_commands for token in command)
 # The relative document identity also cannot be a generator input at the old root.
 relative=path.split('/docs/',1)[1]
 assert not any(x.endswith('/docs/'+relative)for x in tool_paths)
 roles[path]={**d,'actual_compiler_MMD_and_preprocessor_M_rosters_checked':len(rosters),'outside_all_actual_compiler_preprocessor_link_generator_argv':True,'outside_tools_sources_and_generated_input_roles':True,'historical_census_expected_pin_kept_unchanged':True,'current_document_pin_separate':True}
for path,h in plan['pins'].items():assert sha(path)==h,('Current prepared input drift',path)
certificate={'state':'terminal','exit':0,'scope':'Only exact3 documented outside-compiler-closure metadata deltas. Historical whole compile-input censuses remain immutable and are NOT claimed unchanged. Every actual body/header/tool/flag/compiler/preprocessor dependency stays strict. No staging/link/app/ldd.','unique_documentary_deltas':roles,'actual_dependency_rosters':rosters,'actual_command_role_count':len(all_commands),'source_tuple':plan['scope_tuple']}
cert=P/'documentation-role-certificate-v1.json';assert not cert.exists();cert.write_text(json.dumps(certificate,indent=2)+'\n')
plan['inherited_compiler_census_documentation_drift']=list(roles.values());plan['documentation_role_certificate']={'path':str(cert),'sha256':sha(cert),'unique_deltas':3,'genuine_rosters_checked':12,'historical_whole_census_NOT_unchanged':True}
for path in[base,cert,Path(__file__).resolve()]:
 h=sha(path);assert str(path)not in plan['pins']or plan['pins'][str(path)]==h;plan['pins'][str(path)]=h
plan['refined_from_prepared_v2']={'path':str(base),'sha256':sha(base),'link_command_copied_verbatim_unchanged':True,'no_new_binary_input_or_eligibility_relaxation':True}
out=P/'plan-v3.json';assert not out.exists();out.write_text(json.dumps(plan,indent=2)+'\n');print(json.dumps({'state':plan['state'],'plan':str(out),'sha256':sha(out),'certificate_sha256':sha(cert),'summary':plan['summary'],'pins':len(plan['pins'])},indent=2))
