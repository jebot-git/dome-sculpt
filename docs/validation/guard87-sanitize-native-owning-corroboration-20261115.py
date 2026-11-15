"""Disclose exact archive-container differences without normalizing either file."""
from pathlib import Path
import json,hashlib,time
from ninja_audit import sha,archive_members
P=Path(__file__).resolve().parent;A=P/'sanitize-3d1643-completion-v1-audit';E=A/'execution-v1';plan=json.loads((A/'plan.json').read_text());component_path=P.parents[1]/'artifacts/guard87-sanitize-native-component-20261115/component-from-current-whole-objects-v1/component-ready.json';component=json.loads(component_path.read_text());text=(E/'steps.jsonl').read_text();steps=[json.loads(x)for x in text[:text.rfind('\n')+1].splitlines()];done={x['key']:x for x in steps if x['event']=='complete'and x['exit']==0};dest=Path(plan['destination'])
def sections(path):
 rows=[]
 with Path(path).open('rb')as f:
  assert f.read(8)==b'!<arch>\n'
  while True:
   offset=f.tell();h=f.read(60)
   if not h:break
   assert len(h)==60 and h[58:60]==b'`\n';size=int(h[48:58]);hash=hashlib.sha256();left=size
   while left:
    block=f.read(min(left,1024*1024));assert block;hash.update(block);left-=len(block)
   padding=f.read(size%2);rows.append({'offset':offset,'name':h[:16].decode('ascii'),'mtime':h[16:28].decode('ascii'),'uid':h[28:34].decode('ascii'),'gid':h[34:40].decode('ascii'),'mode':h[40:48].decode('ascii'),'size':size,'payload_sha256':hash.hexdigest(),'padding_hex':padding.hex(),'raw_header_hex':h.hex()})
 return rows
results=[]
for key,old in component['archives'].items():
 if key not in done:continue
 actual=dest/key;before=Path(old['path']);assert sha(before)==old['sha256'];assert done[key]['kind']=='link';assert sum(x['event']=='launch'and x['key']==key for x in steps)==1
 expected=[{'name':Path(k).name,'bytes':(dest/k).stat().st_size,'sha256':sha(dest/k)}for k in plan['links'][key]['object_inputs']];members=archive_members(actual);assert members==expected==old['members'],('Actual owning ordered member payload mismatch',key)
 s1=sections(before);s2=sections(actual);assert len(s1)==len(s2);changes=[]
 for a,b in zip(s1,s2):
  delta={k:{'component':a[k],'owning':b[k]}for k in a if a[k]!=b[k]and k!='raw_header_hex'}
  if delta:changes.append({'section':a['name'].strip(),'delta':delta})
 assert all(set(x['delta'])<= {'mtime'} for x in changes),('Archive differences extend beyond disclosed timestamp metadata',key,changes)
 results.append({'archive':key,'whole_container_byte_exact':sha(actual)==old['sha256'],'component_sha256':old['sha256'],'owning_sha256':sha(actual),'ordered_member_payloads_equal':True,'member_count':len(members),'all_section_payloads_equal':all(a['payload_sha256']==b['payload_sha256']for a,b in zip(s1,s2)),'container_differences':changes,'owning_actual_completion':done[key]})
receipt={'scope':'Read-only corroboration of independently produced component archives versus genuinely completed owning archive actions. Whole-file byte comparison fails for nondeterministic ar timestamp metadata; that failure remains. No archive is normalized/rewritten, and no full application pass inferred.','source':plan['source'],'component_receipt':{'path':str(component_path),'sha256':sha(component_path)},'closed_owning_archives':len(results),'expected_native_archives':13,'whole_file_byte_pass':all(x['whole_container_byte_exact']for x in results),'all_ordered_member_payloads_equal':all(x['ordered_member_payloads_equal']for x in results),'all_section_payloads_equal':all(x['all_section_payloads_equal']for x in results),'results':results,'created_epoch':time.time()};out=P/('native-owning-archive-corroboration-'+str(len(results))+'-v1.json');out.write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({k:receipt[k]for k in ['closed_owning_archives','whole_file_byte_pass','all_ordered_member_payloads_equal','all_section_payloads_equal']}));print('receipt',str(out),'SHA',sha(out))
