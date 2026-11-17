"""Read-only sealed CURRENT3d strictSAN authority with explicit 538+494 lineage."""
import json,pathlib,shlex,subprocess
from current_provider_routes_v2 import decide_current_whole_object,new_header_basenames,normalize_actual_command
SOURCE='3d1643827dd56e1b2ef1d57b8a9c6ff1e4211a2c'
PLAN_SHA='9641d8af4976d8937081f5be9d0a999a730196a48c902262ac3c5cff68e39140'
STATUS_SHA='5956c4955b9b831b30a51e8d4b275fc0a2c9a04688a88d024c9f89b368729299'
def journal(path,pin):
 digest=pin(path);launched={};done={};events=[]
 text=pathlib.Path(path).read_text();assert not text or text.endswith('\n'),'Actual journal has an unfinished tail'
 for index,line in enumerate(text.splitlines()):
  event=json.loads(line);events.append(event);table=launched if event['event']=='launch'else done if event['event']=='complete'else None
  assert table is not None and event['key']not in table,('Unexpected/duplicate actual journal entry',index,event)
  table[event['key']]={'line':index+1,**event}
 return {'path':str(path),'sha256':digest,'events':len(events)},launched,done

def seal_context(root,pin,sha):
 base=pathlib.Path(root)/'artifacts/guard87-current-sanitize-completion-20261115';pp=base/'sanitize-3d1643-completion-v1-audit/plan.json'
 def load(p,expected=None):
  value=pin(p)
  if expected is not None:assert value==expected,('Sealed CURRENT authority differs',str(p))
  return json.loads(pathlib.Path(p).read_bytes())
 plan=load(pp,PLAN_SHA);status=load(pp.parent/'execution-v1/status.json',STATUS_SHA)
 assert plan['source']==status['source']==SOURCE and plan['profile']=='sanitize'
 assert status['state']=='terminal'and status['exit']==0 and not status['input_drift']and not status['active']
 assert [status[k]for k in['completed_compilations','completed_links','completed_generators']]==[494,733,3]
 oldsource=pathlib.Path(plan['source_path']);oldbuild=pathlib.Path(plan['destination'])
 assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=oldsource,text=True).strip()==SOURCE and not subprocess.check_output(['git','status','--porcelain'],cwd=oldsource,text=True)
 inputs=load(status['inputs']['path'],status['inputs']['sha256'])
 for p,h in inputs.items():assert pin(p)==h,('Original current input drift',p)
 owned=load(status['owned_outputs']['path'],status['owned_outputs']['sha256']);assert len(owned)==1768
 for key,h in owned.items():assert pin(oldbuild/key)==h
 staged=load(status['staged_whole_providers']['path'],status['staged_whole_providers']['sha256']);assert len(staged)==538
 depref=status['actual_current_dependency_audit'];deps=load(depref['path'],depref['sha256']);assert deps['state']=='passed'and deps['owners']==1032 and not deps['errors']and len(deps['records'])==1032
 currentjournal,currentlaunch,currentdone=journal(status['steps_journal'],pin);assert currentjournal['events']==2460 and len(currentlaunch)==len(currentdone)==1230
 for key,event in currentlaunch.items():
  end=currentdone[key];assert end['exit']==0 and event['pid']==end['pid']and event['kind']==end['kind']
  expected=['nice','-n','10',*plan['objects'][key]['command']]if key in plan['objects']else['nice','-n','10','/bin/bash','-c',(plan['links'].get(key)or plan['generated_outputs'][key])['command']]
  assert event['argv']==expected and pathlib.Path(event['cwd'])==oldbuild
  assert pin(event['log'])==end['log_sha256']
 lineage=plan['interrupted_original'];closed=load(lineage['provider_receipt'],lineage['provider_receipt_sha256']);assert closed['original_session_exit']==143 and closed['original_status_is_stale']and len(closed['closed_objects'])==538
 originalplan=load(closed['original_plan']['path'],closed['original_plan']['sha256']);originalbuild=pathlib.Path(originalplan['destination']);assert originalplan['source']==SOURCE and originalplan['profile']=='sanitize'
 originaljournal,originlaunch,origindone=journal(closed['original_journal']['path'],pin);assert originaljournal['sha256']==closed['original_journal']['sha256']
 actual={};routes={'fresh':0,'prior_closed':0}
 for key,row in plan['objects'].items():
  record=deps['records'][key];assert record['current_owner_route']==row['route']
  if row['route']=='compile-complete-TU':
   assert key not in staged;start=currentlaunch[key];end=currentdone[key];actual_command=row['command'];actual_cwd=oldbuild;origin=currentjournal;routes['fresh']+=1
  else:
   assert row['route']=='reuse-authenticated-whole-object'and key in staged and key not in currentlaunch
   ancestor=closed['closed_objects'][key];start=originlaunch[key];end=origindone[key];actual_command=ancestor['command'];actual_cwd=originalbuild;origin=originaljournal;routes['prior_closed']+=1
   assert pin(ancestor['path'])==ancestor['sha256']==owned[key]and pin(ancestor['depfile'])==ancestor['depfile_sha256']
   assert row['provider']==ancestor['path']and row['provider_sha256']==ancestor['sha256']and row['provider_depfile']==ancestor['depfile']and row['provider_depfile_sha256']==ancestor['depfile_sha256']
   assert normalize_actual_command(actual_command,oldsource,actual_cwd)==normalize_actual_command(row['command'],oldsource,oldbuild)
  assert start['kind']==end['kind']=='compile'and end['exit']==0 and start['pid']==end['pid']
  assert start['argv']==['nice','-n','10',*actual_command]and pathlib.Path(start['cwd'])==actual_cwd and pin(start['log'])==end['log_sha256']
  depfile=pathlib.Path(record['depfile']);assert pin(depfile)==record['depfile_sha256'];raw=depfile.read_text().replace('\\\n',' ');target,body=raw.split(':',1);assert shlex.split(target)==record['actual_target']
  resolved=[str((pathlib.Path(x)if pathlib.Path(x).is_absolute()else actual_cwd/x).resolve())for x in shlex.split(body)];assert resolved==[x['resolved']for x in record['dependencies']],('Actual MD/CWD provenance mismatch',key)
  for entry in record['dependencies']:assert pin(entry['path'])==entry['expected_sha256']
  actual[key]={'kind':'fresh-in-current-completion'if row['route']=='compile-complete-TU'else'closed-current-provider-before-original143','command':actual_command,'argv':start['argv'],'cwd':str(actual_cwd),'journal':origin,'launch':start,'completion':end,'current_declared_equivalence_command':row['command'],'current_actual_MD_snapshot':str(depfile)}
 assert routes=={'fresh':494,'prior_closed':538}
 for p,row in plan['current_toolchain']['tools'].items():assert pin(p)==pin(row['resolved'])==row['sha256']
 for p,h in plan['current_toolchain']['ELF_forecast']['pins'].items():assert pin(p)==h
 aliases={}
 for prior in[plan,originalplan]:
  build=pathlib.Path(prior['destination'])
  for key,row in prior['generated_outputs'].items():
   f=build/key
   if f.is_file():assert pin(f)==row['expected_sha256'];aliases[str(f.resolve())]={'key':key,'sha256':row['expected_sha256']}
 tree={str(pathlib.Path(p).relative_to(oldsource)):h for p,h in plan['source_tree'].items()}
 authority={'plan_path':str(pp),'plan_sha256':PLAN_SHA,'status_path':str(pp.parent/'execution-v1/status.json'),'status_sha256':STATUS_SHA,'source':SOURCE,'old_source_path':str(oldsource),'old_build_path':str(oldbuild),'actual_dependency_manifest':depref,'owned_output_manifest':status['owned_outputs'],'staged_current_manifest':status['staged_whole_providers'],'current_closed_journal':currentjournal,'original_interrupted_journal':originaljournal,'original_actual_exit':143,'original143_not_a_pass':True,'actual_compiler_CWD_routes':routes,'actual_depfiles_rechecked':1032,'old_inputs_rechecked':len(inputs),'old_outputs_rechecked':1768,'genuine_generated_aliases':aliases,'scope':'Only sealed CURRENT3d compiler objects, real MD/CWD and both current owner lineages; no oldcc5 compiler providers.'}
 return {'plan':plan,'plan_path':str(pp),'old_source':oldsource,'old_build':oldbuild,'tree':tree,'owned':owned,'actual_deps':deps,'actual_owner_lineage':actual,'generated_aliases':aliases,'authority':authority}

def refine_objects(context,source,dest,source_pins,objects,generators,sha):
 old=context['plan'];oldsource=context['old_source'];oldbuild=context['old_build'];tree={str(pathlib.Path(p).relative_to(source)):h for p,h in source_pins.items()};assets=lambda t:{k:h for k,h in t.items()if k.startswith('assets/')};sensitive=set()
 for root,rows in[(oldsource,context['tree']),(source,tree)]:
  for name in rows:
   if pathlib.Path(name).suffix not in{'.h','.hpp','.hh','.hxx','.inc','.c','.cc','.cpp'}:continue
   data=(root/name).read_bytes()
   if any(x in data for x in[b'__FILE__',b'__BASE_FILE__',b'__builtin_FILE',b'source_location',b'__TIMESTAMP__',b'__TIME__',b'__DATE__']):sensitive.add(name)
 for key,row in objects.items():
  prior=old['objects'].get(key);record=context['actual_deps']['records'].get(key)
  decision=decide_current_whole_object(key=key,new_command=row['command'],provider_command=prior['command']if prior else None,actual_dependency_record=record,old_source=oldsource,new_source=source,old_build=oldbuild,new_build=dest,new_tree=tree,provider_path=oldbuild/key if key in context['owned']else None,provider_sha256=context['owned'].get(key),current_assets_identical=assets(context['tree'])==assets(tree),generated_aliases=context['generated_aliases'],current_generated_expected={k:v['expected_sha256']for k,v in generators.items()},new_headers=new_header_basenames(context['tree'],tree),source_sensitive_paths=sensitive,hash_file=sha)
  route=decision.pop('route');row.update(decision);row['route']='reuse-authenticated-whole-object'if route=='reuse-authenticated-current-whole-object'else route
  if row['route']=='reuse-authenticated-whole-object':
   lineage=context['actual_owner_lineage'][key];row.update(provider_depfile=record['depfile'],provider_depfile_sha256=record['depfile_sha256'],provider_actual_dependency_manifest=context['authority']['actual_dependency_manifest'],provider_actual_dependency_owner=key,provider_actual_compile_command=lineage['command'],provider_actual_compile_argv=lineage['argv'],provider_actual_compile_cwd=lineage['cwd'],provider_actual_launch=lineage['launch'],provider_actual_completion=lineage['completion'],provider_actual_journal=lineage['journal'],provider_current_declared_equivalence_command=prior['command'],provider_current_owner_lineage=lineage['kind'])
 return {'source_sensitive_paths':sorted(sensitive),'assets_byteidentical':assets(context['tree'])==assets(tree),'literal_D_preserved_including_standalone_operand':True,'actual_current_dependency_body_proof_required':True,'generated_aliases':context['generated_aliases'],'compiler_CWD_routes':context['authority']['actual_compiler_CWD_routes'],'all_final_products_fresh':True}
