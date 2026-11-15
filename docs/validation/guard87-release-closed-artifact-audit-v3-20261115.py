"""Independent read-only audit after sealed driver0/outer-tool143 discrepancy."""
from pathlib import Path
import json,hashlib,subprocess,time
from ninja_audit import sha,archive_members
P=Path(__file__).resolve().parent;A=P/'release-3d1643-current-fresh-v2-audit';E=A/'execution-v1';j=json.loads((A/'plan.json').read_text());s=json.loads((E/'status.json').read_text());started=time.time()
assert sha(A/'plan.json')=='0d03a29a1d2251d1186cb8a9c94f22227566c86664245c51b050a66e458b7dec'
assert s['state']=='terminal'and s['exit']==0 and not s.get('error')and not s['input_drift']and not s['active']
assert [s[k]for k in ['completed_compilations','completed_links','completed_generators']]==[1032,733,3]
log=P/'execute-fresh-v2.log';final=json.loads(log.read_text().splitlines()[-1]);assert final['state']=='terminal'and final['exit']==0 and not final['error']and not final['input_drift']
assert not Path('/proc/2212298').exists()and not Path('/proc/2212279').exists()
source=Path(j['source_path']);assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==j['source'];assert not subprocess.check_output(['git','status','--porcelain'],cwd=source,text=True)
inputs=json.loads((E/'inputs.json').read_text());drift=[k for k,v in inputs.items()if not Path(k).is_file()or sha(k)!=v];assert not drift,drift[:8]
owned_file=Path(s['owned_outputs']['path']);assert sha(owned_file)==s['owned_outputs']['sha256'];owned=json.loads(owned_file.read_text());assert len(owned)==1768 and all(sha(Path(j['destination'])/k)==v for k,v in owned.items())
dep=s['actual_fresh_compiler_dependencies'];f=Path(dep['path']);assert sha(f)==dep['sha256'];d=json.loads(f.read_text());assert d['state']=='passed'and d['owners']==1032 and not d['errors']
for row in d['records'].values():assert sha(row['depfile'])==row['depfile_sha256']
archive_proofs={};component=json.loads((P/'component-from-current-whole-objects-v1/component-ready.json').read_text())
def headers(path):
 rows=[]
 with Path(path).open('rb')as f:
  assert f.read(8)==b'!<arch>\n'
  while h:=f.read(60):
   assert len(h)==60;size=int(h[48:58]);rows.append({'name':h[:16].decode().strip(),'timestamp':h[16:28].decode().strip(),'uid':h[28:34].decode().strip(),'gid':h[34:40].decode().strip(),'mode':h[40:48].decode().strip(),'size':size});f.seek(size+(size%2),1)
 return rows
for k,r in j['links'].items():
 if not r['archive']:continue
 live=Path(j['destination'])/k;wanted=[{'name':Path(x).name,'bytes':(Path(j['destination'])/x).stat().st_size,'sha256':owned[x]}for x in r['object_inputs']];assert archive_members(live)==wanted==component['archives'][k]['members']
 old=Path(component['archives'][k]['path']);a=headers(live);b=headers(old);assert len(a)==len(b)
 diffs=[]
 for n,(aa,bb) in enumerate(zip(a,b)):
  changed=[key for key in aa if aa[key]!=bb[key]];assert set(changed)<= {'mode','timestamp'},(k,n,aa,bb)
  if 'mode'in changed:assert int(aa['mode'],8)&0o222 and not(int(bb['mode'],8)&0o222)
  if changed:diffs.append({'member':aa['name'],'fields':{key:{'owning':aa[key],'component':bb[key]}for key in changed}})
 archive_proofs[k]={'all_ordered_member_payloads_identical':True,'whole_archive_sha_equal':sha(live)==sha(old),'only_archive_header_mtime_mode_differences':diffs}
raw_proof=Path('artifacts/guard87-validation-fresh-v2-20261115/component-owning-archive-payload-audit-v2.json');assert sha(raw_proof)=='74b101a115c0c3f42ba0cf628bb2251b2cf62a78c9bdc31fa438babd0849662a';raw_proof_data=json.loads(raw_proof.read_text());assert raw_proof_data['state']=='terminal'and raw_proof_data['exit']==0 and raw_proof_data['count']==14
result={'raw_archive_symbol_index_payload_proof':{'path':str(raw_proof.resolve()),'sha256':sha(raw_proof)},'state':'terminal','exit':0,'source':j['source'],'scope':'Independent closed artifact/source/tool/header/dependency/archive postcheck. Driver sealed status+printed report0 are verified; outer tool12734 exit143 remains separately preserved and unexplained, not relabeled. This is artifact admission evidence only, no native/GUI/graphics/XR pass.','outer_tool_result':{'session':12734,'exit':143,'cause':'unknown; not inferred from successful driver sealing'},'driver_sealed_result':s,'driver_printed_result':final,'all_current_input_pins':len(inputs),'input_drift':drift,'all_owned_outputs':len(owned),'actual_dependency_receipt':dep,'archives':archive_proofs,'producer_script_sha256':sha(__file__),'seconds':time.time()-started}
out=P/'closed-artifacts-independent-audit-v3.json';assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'state':'terminal','exit':0,'path':str(out),'sha256':sha(out),'owned':len(owned),'dependencies':d['owners'],'archive_payloads':len(archive_proofs),'seconds':result['seconds']},indent=2))
