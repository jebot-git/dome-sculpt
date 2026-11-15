"""Explicit TEST-runtime overlay; frozen App3d remains the product authority."""
import hashlib,json,subprocess
from pathlib import Path
P=Path(__file__).resolve().parent;R=P.parents[1]
RECEIPT=R/'artifacts/source8-runtime-report-object-build-v3-20261115/overlay-admission-v1.json'
EXPECTED='db8780b595f77d8cd4fe818fdee8cfcd9fc5c8dcdd01b915ef3469bd1143080f'
def sha(path):
 with Path(path).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def admit_overlay():
 assert sha(RECEIPT)==EXPECTED
 j=json.loads(RECEIPT.read_text());assert j['state']=='terminal'and j['exit']==0 and j['application_source']=='3d1643827dd56e1b2ef1d57b8a9c6ff1e4211a2c'and j['test_runtime_source']=='c54f66f8feab6ff52331bb264b54fc55cdc14c01'
 assert j['complete_compile_dependency_count']==379 and j['actual_fresh_link_dependency_count']==41 and j['dynamic_exports_exact']and j['dynamic_export_count']==186
 source=R/'artifacts/source8-runtime-report-object-20261115';assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==j['test_runtime_source'];assert not subprocess.check_output(['git','status','--porcelain'],cwd=source,text=True).strip()
 for path,want in j['pins'].items():assert Path(path).is_file()and sha(path)==want,('Changed overlay input/provider',path)
 pins=dict(j['pins']);pins[str(RECEIPT)]=EXPECTED;pins[str(Path(__file__).resolve())]=sha(__file__);return j,pins
