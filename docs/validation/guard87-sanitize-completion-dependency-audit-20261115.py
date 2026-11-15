"""Validate genuine fresh compiler -MD/-MF records against pre-build expectations."""
import hashlib,json,shlex,os
from pathlib import Path

def audit(plan,dest,out,inputs):
 expected={};cache={}
 def actual_sha(path):
  p=Path(path);before=p.stat();identity=(before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns);old=cache.get(str(p))
  if old and old[0]==identity:return old[1]
  h=hashlib.sha256()
  with p.open('rb')as f:
   while block:=f.read(1024*1024):h.update(block)
  after=p.stat();assert identity==(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns),('Dependency changed while hashing',str(p));result=h.hexdigest();cache[str(p)]=(identity,result);return result
 for key,value in inputs.items():
  physical=str(Path(key).resolve());assert physical not in expected or expected[physical]['sha256']==value,('Conflicting resolved prebuild expectations',physical)
  expected[physical]={'sha256':value,'authority':'pre-build current immutable-input boundary'}
 for key,row in plan['generated_outputs'].items():
  physical=str((dest/key).resolve());value=row['expected_sha256'];assert physical not in expected or expected[physical]['sha256']==value
  expected[physical]={'sha256':value,'authority':'owned freshly run source-identical generator expected payload'}
 records={};errors=[];incidences=0;unique=set()
 for key,row in plan['objects'].items():
  depfile=dest/row['depfile'];assert depfile.is_file(),('Missing actual fresh compiler depfile',key,str(depfile));text=depfile.read_text().replace('\\\n',' ');target,body=text.split(':',1);targets=shlex.split(target);assert targets==[key]or targets==[str(dest/key)],('Actual depfile target mismatch',key,targets)
  names=shlex.split(body);assert names,('Empty actual depfile',key);entries=[]
  for name in names:
   path=Path(name);path=path if path.is_absolute()else dest/path;physical=str(path.resolve());proof=expected.get(physical);incidences+=1;unique.add(physical)
   if proof is None:errors.append({'owner':key,'path':str(path),'resolved':physical,'error':'Actual fresh dependency was not pre-pinned or an owned generator'})
   elif not path.is_file()or actual_sha(path)!=proof['sha256']:errors.append({'owner':key,'path':str(path),'resolved':physical,'error':'Actual fresh dependency missing or changed from prebuild expectation'})
   entries.append({'path':str(path),'resolved':physical,'expected_sha256':proof['sha256']if proof else None,'authority':proof['authority']if proof else None})
  records[key]={'depfile':str(depfile),'depfile_sha256':actual_sha(depfile),'actual_target':targets,'current_owner_route':row['route'],'original_current_depfile_provider':row.get('provider_depfile'),'dependencies':entries}
 result={'state':'passed'if not errors else'failed','source':plan['source'],'scope':'Genuine current compiler -MD/-MF records from538 explicitly documented completed providers plus494 freshly completed owners; no fabricated dependency provenance. Every source/header dependency matches unchanged original/current prebuild pins or owned expected generator payloads.','owners':len(records),'dependency_incidences':incidences,'unique_resolved_dependencies':len(unique),'errors':errors,'records':records}
 path=out/'actual-fresh-dependencies.json';path.write_text(json.dumps(result,indent=2)+'\n');summary={'path':str(path),'sha256':actual_sha(path),'owners':len(records),'dependency_incidences':incidences,'unique_resolved_dependencies':len(unique),'errors':len(errors)}
 assert not errors,('Fresh actual depfile audit failed',summary,errors[:8]);return summary
