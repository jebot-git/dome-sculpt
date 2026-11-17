"""Audit strict-SAN genuine reused MD from both old compiler CWDs and new fresh MD.
PREPARE-ONLY template; invoke only inside an independently reviewed owning build.
"""
import hashlib,json,os,shlex,stat,time
from pathlib import Path
from current_provider_routes_v2 import decide_current_whole_object,new_header_basenames,normalize_actual_command
MINIMUM_SENSITIVE={'third_party/desktop/bdf_font.hpp','src/vroid-creator.cpp','tests/layered-normal-native.cpp','tests/source-rest-ui-fixture.cpp','tests/avatar.cpp','third_party/ufbx/ufbx.c','third_party/xatlas/xatlas.cpp'}
def file_identity(path):
 s=Path(path).stat();return {'dev':s.st_dev,'ino':s.st_ino,'bytes':s.st_size,'mode':stat.S_IMODE(s.st_mode),'mtime_ns':s.st_mtime_ns,'ctime_ns':s.st_ctime_ns}
def parse_actual_md(path,cwd,key):
 """Read unchanged actual bytes, resolving names in the ACTUAL compiler cwd."""
 raw=Path(path).read_bytes();text=raw.decode().replace('\\\n',' ');target,body=text.split(':',1);targets=shlex.split(target)
 assert targets==[key]or targets==[str(Path(cwd)/key)],('Actual compiler target mismatch',key,targets)
 names=shlex.split(body);assert names,('Actual dependency record is empty',key)
 entries=[]
 for name in names:
  actual=Path(name);actual=actual if actual.is_absolute()else Path(cwd)/actual
  entries.append({'literal':name,'actual_path':str(actual),'resolved':str(actual.resolve())})
 return {'raw_sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw),'actual_target':targets,'actual_cwd':str(cwd),'dependencies':entries}
def actual_jobs(path):
 launched={};done={}
 for line_number,line in enumerate(Path(path).read_text().splitlines(),1):
  event=json.loads(line);table=launched if event['event']=='launch'else done if event['event']=='complete'else None
  if table is None:continue # Explicit stage/cleanup records never become compiler completion.
  key=event['key'];assert key not in table,('Duplicate actual job event',key,event['event'])
  table[key]={'journal_line':line_number,**event}
 return launched,done

def audit(plan,dest,out,inputs,staged,journal,checkpoint):
 dest=Path(dest);out=Path(out);source=Path(plan['source_path']);expected={};cache={};records={};error=None;counts={'fresh':0,'reused':0};incidences=0;unique=set()
 def sha(path):
  checkpoint();p=Path(path);before=p.stat();ident=(before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns);prior=cache.get(str(p))
  if prior and prior[0]==ident:return prior[1]
  h=hashlib.sha256()
  with p.open('rb')as f:
   while b:=f.read(1<<20):checkpoint();h.update(b)
  after=p.stat();assert ident==(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns),('Dependency changed during audit',str(p))
  value=h.hexdigest();cache[str(p)]=(ident,value);return value
 def load_pinned(path,digest):
  assert expected[str(Path(path).resolve())]['sha256']==digest and sha(path)==digest,('Unpinned/changed actual receipt',str(path))
  return json.loads(Path(path).read_bytes())
 def readonly_copy_matches(path,provider,digest):
  a=Path(path).lstat();b=Path(provider).stat();assert stat.S_ISREG(a.st_mode)and not(a.st_mode&0o222)and a.st_nlink==1
  assert(a.st_dev,a.st_ino)!=(b.st_dev,b.st_ino),('Staged copy aliases provider inode',str(path))
  assert sha(path)==sha(provider)==digest
 for path,digest in inputs.items():
  p=str(Path(path).resolve());assert p not in expected or expected[p]['sha256']==digest,('Conflicting physical input pin',p)
  expected[p]={'sha256':digest,'authority':'immutable pre-build input pin'}
 launched,done=actual_jobs(journal)
 for key,row in plan['generated_outputs'].items():
  checkpoint();p=str((dest/key).resolve());digest=row['expected_sha256'];assert p not in expected or expected[p]['sha256']==digest
  assert launched[key]['kind']==done[key]['kind']=='generate'and done[key]['exit']==0 and launched[key]['pid']==done[key]['pid']
  assert launched[key]['actual_command']==row['command']and launched[key]['cwd']==str(dest)
  assert sha(dest/key)==digest
  expected[p]={'sha256':digest,'authority':'actual freshly completed owned generator'}
 authority=plan['provider_authority'];oldplan=load_pinned(authority['plan_path'],authority['plan_sha256']);oldsource=Path(oldplan['source_path']);oldbuild=Path(oldplan['destination'])
 olddeps=load_pinned(authority['actual_dependency_manifest']['path'],authority['actual_dependency_manifest']['sha256'])
 oldowned=load_pinned(authority['owned_output_manifest']['path'],authority['owned_output_manifest']['sha256'])
 assert plan['profile']==oldplan['profile']=='sanitize'and authority['original_actual_exit']==143 and authority['original143_not_a_pass']is True
 assert authority['actual_compiler_CWD_routes']=={'fresh':494,'prior_closed':538}
 closed_ref=oldplan['interrupted_original'];closed=load_pinned(closed_ref['provider_receipt'],closed_ref['provider_receipt_sha256'])
 assert closed['original_session_exit']==143 and closed['original_status_is_stale']and len(closed['closed_objects'])==538
 original_ref=closed['original_plan'];originalplan=load_pinned(original_ref['path'],original_ref['sha256']);originalbuild=Path(originalplan['destination'])
 assert originalplan['source']==oldplan['source']and originalplan['source_path']==oldplan['source_path']and originalplan['profile']=='sanitize'
 journals={}
 for kind,ref in [('fresh-in-current-completion',authority['current_closed_journal']),('closed-current-provider-before-original143',authority['original_interrupted_journal'])]:
  assert sha(ref['path'])==ref['sha256']and expected[str(Path(ref['path']).resolve())]['sha256']==ref['sha256']
  launches,ends=actual_jobs(ref['path']);journals[kind]={'ref':ref,'launches':launches,'ends':ends}
 current_journal=journals['fresh-in-current-completion'];assert len(current_journal['launches'])==len(current_journal['ends'])==1230
 assert authority['original_interrupted_journal']['path']==closed['original_journal']['path']and authority['original_interrupted_journal']['sha256']==closed['original_journal']['sha256']
 oldroutes={'fresh':sum(r['route']=='compile-complete-TU'for r in oldplan['objects'].values()),'prior_closed':sum(r['route']=='reuse-authenticated-whole-object'for r in oldplan['objects'].values())}
 assert oldroutes=={'fresh':494,'prior_closed':538}
 newtree={str(Path(p).relative_to(source)):digest for p,digest in plan['source_tree'].items()}
 oldtree={str(Path(p).relative_to(oldsource)):digest for p,digest in oldplan['source_tree'].items()}
 assets=lambda t:{k:h for k,h in t.items()if k.startswith('assets/')}
 sensitive=set(MINIMUM_SENSITIVE)
 for root,tree in[(oldsource,oldtree),(source,newtree)]:
  for name in tree:
   checkpoint()
   if Path(name).suffix not in{'.h','.hpp','.hh','.hxx','.inc','.cpp','.cc','.c'}:continue
   data=(root/name).read_bytes()
   if any(x in data for x in[b'__FILE__',b'__BASE_FILE__',b'__builtin_FILE',b'source_location',b'__TIMESTAMP__',b'__TIME__',b'__DATE__']):sensitive.add(name)
 aliases={}
 for prior in [oldplan,originalplan]:
  build=Path(prior['destination'])
  for key,row in prior['generated_outputs'].items():
   path=build/key
   if path.is_file():
    digest=row['expected_sha256'];assert sha(path)==digest and expected[str(path.resolve())]['sha256']==digest
    aliases[str(path.resolve())]={'key':key,'sha256':digest}
 assert aliases==authority['genuine_generated_aliases']==plan['provider_equivalence_policy']['generated_aliases']
 current_generated={k:row['expected_sha256']for k,row in plan['generated_outputs'].items()}
 # Exact source/generator input roles must remain proved independently of equal-byte aliases.
 for key,row in plan['generated_outputs'].items():
  prior=oldplan['generated_outputs'][key]
  for entry in row['inputs']:
   p=Path(entry['current']);assert sha(p)==entry['sha256']and expected[str(p.resolve())]['sha256']==entry['sha256']
   if p.is_relative_to(source):
    oldpath=oldsource/p.relative_to(source);matches=[x for x in prior['inputs']if Path(x['current'])==oldpath]
    assert len(matches)==1 and matches[0]['sha256']==entry['sha256']
  assert prior['expected_sha256']==row['expected_sha256']
 progress_path=out/'actual-current-owner-dependencies-prefix.jsonl';progress=progress_path.open('x');prefix_rows=0
 try:
  for key,row in plan['objects'].items():
   checkpoint();route=row['route'];assert route in{'compile-complete-TU','reuse-authenticated-whole-object'}
   if route=='compile-complete-TU':
    assert key not in staged and launched[key]['kind']==done[key]['kind']=='compile'and done[key]['exit']==0 and launched[key]['pid']==done[key]['pid']
    assert launched[key]['actual_command']==row['command']and launched[key]['argv']==['nice','-n','10',*row['command']]and launched[key]['cwd']==str(dest)
    assert row['command'][row['command'].index('-c')+1]==row['source']
    depfile=dest/row['depfile'];parsed=parse_actual_md(depfile,dest,key);entries=[]
    for dep in parsed['dependencies']:
     checkpoint();p=Path(dep['resolved']);assert not p.is_relative_to(oldsource)and not p.is_relative_to(oldbuild)and not p.is_relative_to(originalbuild),('Fresh owner read old-source/build dependency',key,str(p))
     proof=expected.get(str(p));assert proof is not None and sha(dep['actual_path'])==proof['sha256'],('Fresh actual dependency missing/changed/unpinned',key,dep)
     entries.append({**dep,'expected_sha256':proof['sha256'],'authority':proof['authority']})
    records[key]={'route':route,'newly_compiled':True,'actual_compile_argv':launched[key]['argv'],'actual_compile_cwd':str(dest),'depfile':str(depfile),'actual_MD':{k:v for k,v in parsed.items()if k!='dependencies'},'dependencies':entries};counts['fresh']+=1
   else:
    assert key in staged and key not in launched and key not in done,('Reused owner has a new compiler event',key)
    assert not(dest/row['depfile']).exists(),('Reused owner masquerades as a new compiler .d',key)
    stage=staged[key];assert stage['route']==route and stage['newly_compiled']is False
    assert stage['provider']==row['provider']and stage['provider_sha256']==row['provider_sha256']
    assert Path(row['provider'])==oldbuild/key and row['provider_sha256']==oldowned[key]
    readonly_copy_matches(dest/key,row['provider'],row['provider_sha256'])
    assert file_identity(row['provider'])==stage['object_copy']['provider_identity_before']==stage['object_copy']['provider_identity_after']
    assert file_identity(dest/key)==stage['object_copy']['copy_identity']
    assert stage['original_depfile']==row['provider_depfile']and stage['original_depfile_sha256']==row['provider_depfile_sha256']
    original=Path(row['provider_depfile']);snapshot=Path(stage['unchanged_depfile_snapshot']);readonly_copy_matches(snapshot,original,row['provider_depfile_sha256'])
    assert file_identity(original)==stage['depfile_copy']['provider_identity_before']==stage['depfile_copy']['provider_identity_after']
    assert file_identity(snapshot)==stage['depfile_copy']['copy_identity']
    assert snapshot.resolve().is_relative_to((out/'provider-depfiles').resolve())
    prior=oldplan['objects'][key];sealed=olddeps['records'][key]
    assert str(original)==sealed['depfile']and row['provider_actual_dependency_owner']==key and sealed['current_owner_route']==prior['route']
    assert row['provider_current_declared_equivalence_command']==prior['command']
    if prior['route']=='compile-complete-TU':
     lineage='fresh-in-current-completion';actual_cwd=oldbuild;actual_command=prior['command'];actual_compiler_md=original
    else:
     assert prior['route']=='reuse-authenticated-whole-object'
     lineage='closed-current-provider-before-original143';actual_cwd=originalbuild;ancestor=closed['closed_objects'][key];actual_command=ancestor['command'];actual_compiler_md=Path(ancestor['depfile'])
     assert sha(ancestor['path'])==ancestor['sha256']==oldowned[key]and sha(actual_compiler_md)==ancestor['depfile_sha256']==sealed['depfile_sha256']
     assert prior['provider']==ancestor['path']and prior['provider_depfile']==ancestor['depfile']and prior['provider_sha256']==ancestor['sha256']and prior['provider_depfile_sha256']==ancestor['depfile_sha256']
     assert normalize_actual_command(actual_command,oldsource,actual_cwd)==normalize_actual_command(prior['command'],oldsource,oldbuild)
    oldjournal=journals[lineage]['ref'];a=journals[lineage]['launches'][key];b=journals[lineage]['ends'][key]
    assert row['provider_current_owner_lineage']==lineage and row['provider_actual_journal']==oldjournal
    assert a['kind']==b['kind']=='compile'and b['exit']==0 and a['pid']==b['pid']
    assert a['argv']==row['provider_actual_compile_argv']==['nice','-n','10',*actual_command]
    assert row['provider_actual_compile_command']==actual_command and a['cwd']==row['provider_actual_compile_cwd']==str(actual_cwd)
    for declared,actual in [(row['provider_actual_launch'],a),(row['provider_actual_completion'],b)]:
     for field in ['event','key','kind','pid','argv','cwd','exit','log','log_sha256']:
      if field in declared or field in actual:assert declared.get(field)==actual.get(field),('Original actual event changed',key,field)
    assert sha(a['log'])==b['log_sha256']
    assert stage['original_actual_compile_command']==actual_command and stage['original_actual_compile_argv']==a['argv']and stage['original_actual_compile_cwd']==str(actual_cwd)and stage['original_actual_journal']==oldjournal
    parsed=parse_actual_md(snapshot,actual_cwd,key);assert parsed['raw_sha256']==sha(original)==sha(actual_compiler_md)==row['provider_depfile_sha256']==sealed['depfile_sha256']
    assert parsed['actual_target']==sealed['actual_target']and [x['resolved']for x in parsed['dependencies']]==[x['resolved']for x in sealed['dependencies']]
    decision=decide_current_whole_object(key=key,new_command=row['command'],provider_command=prior['command'],actual_dependency_record=sealed,old_source=oldsource,new_source=source,old_build=oldbuild,new_build=dest,new_tree=newtree,provider_path=row['provider'],provider_sha256=row['provider_sha256'],current_assets_identical=assets(oldtree)==assets(newtree),generated_aliases=aliases,current_generated_expected=current_generated,new_headers=new_header_basenames(oldtree,newtree),source_sensitive_paths=sensitive,hash_file=sha)
    assert not decision['reasons']and decision['route']=='reuse-authenticated-current-whole-object',('Reused owner lost equivalence',key,decision['reasons'])
    equivalents=decision['actual_dependency_checks'];assert len(equivalents)==len(parsed['dependencies'])
    entries=[]
    for dep,prior_dep,equivalence in zip(parsed['dependencies'],sealed['dependencies'],equivalents):
     checkpoint();digest=prior_dep['expected_sha256'];assert digest and sha(dep['actual_path'])==digest
     current=equivalence['current'];proof=expected.get(str(Path(current).resolve()));assert proof is not None and proof['sha256']==digest and sha(current)==digest
     entries.append({**dep,'original_expected_sha256':digest,'actual_original_CWD':str(actual_cwd),'current_equivalent':{'path':current,'resolved':str(Path(current).resolve()),'sha256':digest,'kind':equivalence['kind'],'authority':proof['authority']}})
    records[key]={'route':route,'newly_compiled':False,'provider':row['provider'],'original_actual_compile_argv':a['argv'],'original_actual_compile_cwd':str(actual_cwd),'provider_current_owner_lineage':lineage,'true_original_compiler_depfile':str(actual_compiler_md),'current_completion_MD_snapshot':str(original),'original_actual_journal':oldjournal,'original_actual_launch':a,'original_actual_completion':b,'original_depfile':str(original),'unchanged_snapshot':str(snapshot),'actual_original_MD':{k:v for k,v in parsed.items()if k!='dependencies'},'current_source_equivalence_separate_from_MD':True,'dependencies':entries};counts['reused']+=1
   incidences+=len(records[key]['dependencies']);unique.update(x['resolved']for x in records[key]['dependencies'])
   progress.write(json.dumps({'owner':key,**records[key]})+'\n');progress.flush();prefix_rows+=1
  assert len(records)==plan['summary']['configured_TUs']==1036
  assert counts['fresh']==plan['summary']['fresh_complete_TUs']and counts['reused']==plan['summary']['reused_whole_objects']
  assert counts['fresh']+counts['reused']==1036 and len(staged)==counts['reused']
  assert len([x for x in launched.values()if x['kind']=='compile'])==counts['fresh']
  assert len([x for x in launched.values()if x['kind']=='link'])==len(plan['links'])==737
  assert len([x for x in launched.values()if x['kind']=='generate'])==len(plan['generated_outputs'])==3
  assert set(launched)==set(done)and all(x['exit']==0 for x in done.values())
 except BaseException as exc:error=repr(exc);raise
 finally:
  progress.close()
  report={'state':'failed'if error else'passed','source':plan['source'],'scope':'Separate genuine original strict-SAN MD reused-owner records from the 538 original / 494 completion lineage and genuine newly compiled MD records. Original dependency bytes, names, physical old paths and command/CWD preserved; current source/generated equivalents are separate evidence. Reused owners are never called newly compiled.','owners_visited':len(records),'configured_owners_expected':1036,'fresh_compiler_owners':counts['fresh'],'reused_current_whole_owners':counts['reused'],'dependency_incidences':incidences,'unique_original_resolved_dependencies':len(unique),'errors':[error]if error else[],'complete_prefix_rows':prefix_rows,'prefix_stream':str(progress_path),'prefix_tail_may_be_partial_on_failure':bool(error),'records':{}if error else records,'failure_record_payload_scope':'On failure see preserved JSONL prefix; do not spend TERM grace serializing large in-memory prefix'if error else None}
  path=out/'actual-current-owner-dependencies.json';temp=path.with_suffix('.writing');temp.write_text(json.dumps(report,indent=2)+'\n');temp.replace(path)
 return {'path':str(path),'sha256':sha(path),'owners':len(records),'fresh_compiler_owners':counts['fresh'],'reused_current_whole_owners':counts['reused'],'dependency_incidences':incidences,'errors':0}
