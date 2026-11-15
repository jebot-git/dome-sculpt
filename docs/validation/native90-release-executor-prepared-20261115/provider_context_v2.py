"""Read-only Release3d current-provider authority; original outer143 stays143."""
import hashlib,json,pathlib,shlex,subprocess
from current_provider_routes_v2 import decide_current_whole_object,new_header_basenames
ADMISSION_SHA='39c71b267c63127b68b40acc959df021fd662a56fad38c809d0a92af741c6f3e'
PLAN_SHA='0d03a29a1d2251d1186cb8a9c94f22227566c86664245c51b050a66e458b7dec'
SOURCE='3d1643827dd56e1b2ef1d57b8a9c6ff1e4211a2c'
def seal_context(root,pin,sha):
 base=pathlib.Path(root)/'artifacts/current-toolchain-guard87-release-20261115'
 def load(p,expected=None):
  actual=pin(p)
  if expected is not None:assert actual==expected,('Provider authority digest differs',str(p))
  return json.loads(pathlib.Path(p).read_bytes())
 admission_path=base/'closed-artifacts-independent-audit-v3.json';admission=load(admission_path,ADMISSION_SHA)
 assert admission['state']=='terminal'and admission['exit']==0 and admission['source']==SOURCE and not admission['input_drift']
 pp=base/'release-3d1643-current-fresh-v2-audit/plan.json';plan=load(pp,PLAN_SHA);oldsource=pathlib.Path(plan['source_path']);oldbuild=pathlib.Path(plan['destination'])
 status=load(pp.parent/'execution-v1/status.json');assert status==admission['driver_sealed_result']and status['exit']==0 and status['source']==SOURCE and not status['input_drift']
 assert status['completed_compilations']==1032 and status['completed_links']==733 and status['completed_generators']==3
 assert admission['outer_tool_result']['session']==12734 and admission['outer_tool_result']['exit']==143
 assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=oldsource,text=True).strip()==SOURCE
 assert not subprocess.check_output(['git','status','--porcelain'],cwd=oldsource,text=True).strip()
 original_inputs=load(status['inputs']['path'],status['inputs']['sha256']);assert len(original_inputs)==30961
 for p,h in original_inputs.items():assert pin(p)==h,('Original current input drift',p)
 owned=load(status['owned_outputs']['path'],status['owned_outputs']['sha256']);assert len(owned)==1768
 for k,h in owned.items():assert pin(oldbuild/k)==h,('Original output drift',k)
 deps_ref=status['actual_fresh_compiler_dependencies'];deps=load(deps_ref['path'],deps_ref['sha256']);assert deps['owners']==1032 and not deps['errors']and len(deps['records'])==1032
 for key,row in deps['records'].items():
  p=pathlib.Path(row['depfile']);assert pin(p)==row['depfile_sha256'];text=p.read_text().replace('\\\n',' ');target,body=text.split(':',1)
  assert shlex.split(target)==row['actual_target']
  actual=[str((pathlib.Path(x)if pathlib.Path(x).is_absolute()else oldbuild/x).resolve())for x in shlex.split(body)]
  assert actual==[x['resolved']for x in row['dependencies']],('Actual raw .d differs',key)
  for dep in row['dependencies']:assert pin(dep['path'])==dep['expected_sha256']
 for p,row in plan['current_toolchain']['tools'].items():assert pin(p)==row['sha256']and pin(row['resolved'])==row['sha256']
 for p,h in plan['current_toolchain']['ELF_forecast']['pins'].items():assert pin(p)==h
 # This is the genuine closed owning journal, observed and pinned now. Do not invent a historical journal digest.
 journal=pathlib.Path(status['steps_journal']);journal_sha=pin(journal);launches={};completions={};events=[json.loads(x)for x in journal.read_text().splitlines()]
 for index,event in enumerate(events):
  table=launches if event['event']=='launch'else completions if event['event']=='complete'else None
  assert table is not None and event['key']not in table,('Duplicate/unexpected closed actual job event',index,event)
  table[event['key']]={'line':index+1,**event}
 assert len(events)==3536 and len(launches)==len(completions)==1768
 for key in set(plan['objects'])|set(plan['links'])|set(plan['generated_outputs']):
  launched=launches[key];done=completions[key];assert launched['pid']==done['pid']and done['exit']==0 and launched['kind']==done['kind']
  expected=['nice','-n','10',*plan['objects'][key]['command']]if key in plan['objects']else['nice','-n','10','/bin/bash','-c',(plan['links'].get(key)or plan['generated_outputs'][key])['command']]
  assert launched['argv']==expected and pathlib.Path(launched['cwd'])==oldbuild
  assert pin(launched['log'])==done['log_sha256']
 tree={str(pathlib.Path(p).relative_to(oldsource)):h for p,h in plan['source_tree'].items()}
 return {'plan':plan,'plan_path':str(pp),'old_source':oldsource,'old_build':oldbuild,'tree':tree,'owned':owned,'actual_deps':deps,'actual_launches':launches,'actual_completions':completions,'authority':{'admission_path':str(admission_path),'admission_sha256':ADMISSION_SHA,'plan_sha256':PLAN_SHA,'plan_path':str(pp),'old_source_path':str(oldsource),'old_build_path':str(oldbuild),'source':SOURCE,'original_outer':admission['outer_tool_result'],'actual_depfiles_rechecked':1032,'old_inputs_rechecked':30961,'old_outputs_rechecked':1768,'actual_dependency_manifest':deps_ref,'owned_output_manifest':status['owned_outputs'],'actual_closed_journal':{'path':str(journal),'sha256':journal_sha,'events':len(events),'observed_snapshot_scope':'Genuine complete original owning journal pinned at successor preparation; not an invented historical digest'}}}
def refine_objects(context,source,dest,source_pins,objects,generators,sha):
 old=context['plan'];oldsource=context['old_source'];oldbuild=context['old_build'];tree={str(pathlib.Path(p).relative_to(source)):h for p,h in source_pins.items()}
 assets=lambda t:{k:h for k,h in t.items()if k.startswith('assets/')}
 sameassets=assets(context['tree'])==assets(tree)
 sensitive=set()
 for root,rows in[(oldsource,context['tree']),(source,tree)]:
  for name in rows:
   if pathlib.Path(name).suffix not in{'.h','.hpp','.hh','.hxx','.inc','.c','.cc','.cpp'}:continue
   data=(root/name).read_bytes()
   if any(x in data for x in[b'__FILE__',b'__BASE_FILE__',b'__builtin_FILE',b'source_location',b'__TIMESTAMP__',b'__TIME__',b'__DATE__']):sensitive.add(name)
 aliases={str((oldbuild/k).resolve()):{'key':k,'sha256':v['expected_sha256']}for k,v in old['generated_outputs'].items()}
 expected={k:v['expected_sha256']for k,v in generators.items()}
 for key,row in objects.items():
  prior=old['objects'].get(key);actual=context['actual_deps']['records'].get(key)
  decision=decide_current_whole_object(key=key,new_command=row['command'],provider_command=prior['command']if prior else None,actual_dependency_record=actual,old_source=oldsource,new_source=source,old_build=oldbuild,new_build=dest,new_tree=tree,provider_path=oldbuild/key if key in context['owned']else None,provider_sha256=context['owned'].get(key),current_assets_identical=sameassets,generated_aliases=aliases,current_generated_expected=expected,new_headers=new_header_basenames(context['tree'],tree),source_sensitive_paths=sensitive,hash_file=sha)
  route=decision.pop('route');row.update(decision);row['route']='reuse-authenticated-whole-object'if route=='reuse-authenticated-current-whole-object'else route
  if row['route']=='reuse-authenticated-whole-object':
   row.update(provider_depfile=actual['depfile'],provider_depfile_sha256=actual['depfile_sha256'],provider_actual_dependency_manifest=context['authority']['actual_dependency_manifest'],provider_actual_dependency_owner=key,provider_actual_compile_command=prior['command'],provider_actual_compile_argv=context['actual_launches'][key]['argv'],provider_actual_compile_cwd=context['actual_launches'][key]['cwd'],provider_actual_launch=context['actual_launches'][key],provider_actual_completion=context['actual_completions'][key],provider_actual_journal=context['authority']['actual_closed_journal'])
 return {'source_sensitive_paths':sorted(sensitive),'assets_byteidentical':sameassets,'literal_D_preserved_including_standalone_operand':True,'actual_current_dependency_body_proof_required':True,'generated_aliases':aliases,'all_final_products_fresh':True}
