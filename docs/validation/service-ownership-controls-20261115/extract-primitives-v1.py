from pathlib import Path
import ast,textwrap,hashlib,json,difflib
P=Path(__file__).resolve().parent;R=P.parents[1];source=R/'artifacts/native90-release-planning-20261115/execute-release-native90-v1.py';raw=source.read_text();expected='2e6477ef1156d074ba6d1c6ed0e57a611b94ada0a8e70648a4395af4d7711d1a';assert hashlib.sha256(source.read_bytes()).hexdigest()==expected
module=ast.parse(raw);top={x.name:x for x in module.body if isinstance(x,(ast.FunctionDef,ast.ClassDef))};nested={x.name:x for x in top['execute'].body if isinstance(x,ast.FunctionDef)};lines=raw.splitlines(True)
def body(node):return textwrap.dedent(''.join(lines[node.lineno-1:node.end_lineno]))
standalone=['OwnedTermination','received_signal','proc_identity','file_identity','atomic','descendants'];closed=['service_pids','observe','sample','checkpoint','cleanup'];old={name:body((top if name in standalone else nested)[name])for name in standalone+closed};new=dict(old)
new['sample']=new['sample'].replace("peak<=8*2**30,'Owned executor+compiler aggregateRSS exceeds8GiB'","peak<=rss_limit,'Owned control aggregateRSS exceeds declared test limit'")
new['checkpoint']=new['checkpoint'].replace("'Declared18000s complete materialization bound exceeded'","'Declared control monotonic deadline exceeded'")
header='''"""Exact extracted ownership/resource primitives for isolated real CPU controls.
Not imported by any held executor; no whole-mixed-executor qualification claim.
Only RSS threshold and deadline error label are explicitly parameterized below.
"""
import os,signal,subprocess,time,stat,json
from pathlib import Path
'''
factory='''\ndef make_primitives(*,cgroup_root,active,owned,ledger,state,record,deadline,rss_limit):
 peak=0;next_sample=0
'''
for name in closed:factory+=textwrap.indent(new[name],' ')+'\n'
factory+=" return {'service_pids':service_pids,'observe':observe,'sample':sample,'checkpoint':checkpoint,'cleanup':cleanup,'peak':lambda:peak}\n"
output=header+'\n'.join(new[name]for name in standalone)+factory;(P/'ownership_primitives_v1.py').write_text(output)
changes=''
for name in closed:changes+=''.join(difflib.unified_diff(old[name].splitlines(True),new[name].splitlines(True),fromfile='held:'+name,tofile='control:'+name))
(P/'explicit-primitive-parameters-v1.diff').write_text(changes)
receipt={'held_source':str(source),'held_source_sha256':expected,'extraction_only_no_held_mutation':True,'functions':{n:{'held_sha256':hashlib.sha256(old[n].encode()).hexdigest(),'control_sha256':hashlib.sha256(new[n].encode()).hexdigest(),'byte_identical_after_dedent':old[n]==new[n]}for n in old},'control_module_sha256':hashlib.sha256((P/'ownership_primitives_v1.py').read_bytes()).hexdigest(),'scope':'Operational process ownership/resource primitives only. No compiler, staging, .o/.d or fake build/Ninja journals.'};(P/'extraction-receipt-v1.json').write_text(json.dumps(receipt,indent=2)+'\n');print(receipt['control_module_sha256'])
