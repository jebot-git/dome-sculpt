"""Additive App1bc component shim; preserve unchanged full old3d admission/pins."""
import importlib.util,json,sys
from pathlib import Path
P=Path(__file__).resolve().parent;R=P.parents[1];OLD=R/'artifacts/guard87-validation-admitted-v3-20261115'
paths=list(sys.path);sys.path.insert(0,str(OLD))
try:
 spec=importlib.util.spec_from_file_location('unchanged_guard87_original_cohort',OLD/'cohort.py');_original=importlib.util.module_from_spec(spec);spec.loader.exec_module(_original)
finally:sys.path[:]=paths
assert Path(sys.modules['runtime_contract'].__file__).resolve()==OLD/'runtime_contract.py'
S,B,HEAD,TARGETS,G,sha,write,verify=(_original.__dict__[key]for key in['S','B','HEAD','TARGETS','G','sha','write','verify'])
from runtime_contract import merge_pins
LINK=R/'artifacts/app1bc-public-component-link-20261115';LOADER=R/'artifacts/app1bc-component-loader-20261115'
APP=LINK/'trial-v2/dome-sculpt';APP_SHA='4988652c32940b4506fcceb66db6776f0949018972c8bdcc239421845ccdb042';ADMISSION=LOADER/'component-loader-admission-v1.json';ADMISSION_SHA='cf5f61a82d2931c19205314e8b55c550d535377746f2aa199a2dcb7558885119'
def admit(profile):
 old=_original.admit(profile);pins=dict(old['pins']);assert sha(ADMISSION)==ADMISSION_SHA;component=json.loads(ADMISSION.read_text());assert component['state']=='admitted-component-loader-and-version-only'and component['exit']==0 and not component['input_drift']
 assert component['application']['path']==str(APP)and component['application']['sha256']==APP_SHA and sha(APP)==APP_SHA
 plan=LINK/'plan-v3.json';assert sha(plan)=='7935daa0e064eb9656fa90a63de567742dbb430a21615ef4cdac1f068aeaff3c';declared=json.loads(plan.read_text())
 assert declared['scope_tuple']=={'application_product':'1bc13125','unchanged_component_context':'3d164382','literal_asset_context':'3d164382'}
 for path,digest in[(LINK/'execution-v1/status.json',component['producer_status_sha256']),(LOADER/'status.json',component['loader_status_sha256']),(LINK/'root-link-review-v3.json',component['root_link_review_sha256'])]:assert sha(path)==digest
 status=json.loads((LINK/'execution-v1/status.json').read_text());loader=json.loads((LOADER/'status.json').read_text());assert status['state']==loader['state']=='terminal'and status['exit']==loader['exit']==0 and not status['input_drift']and not loader['input_drift']
 merge_pins(pins,declared['pins']);merge_pins(pins,loader['pins']);merge_pins(pins,{str(ADMISSION):ADMISSION_SHA,str(plan):sha(plan),str(LINK/'execution-v1/status.json'):sha(LINK/'execution-v1/status.json'),str(LOADER/'status.json'):sha(LOADER/'status.json'),str(LOADER/'actual-loader-providers.json'):sha(LOADER/'actual-loader-providers.json'),str(P/'cohort.py'):sha(P/'cohort.py')})
 assert str(APP)not in old['pins']and old['targets']['app']['path']!=str(APP),'Never overwrite original App expectation'
 targets={key:dict(row)for key,row in old['targets'].items()};original_target=dict(targets['app']);targets['app']={'path':str(APP),'sha256':APP_SHA,'source_role':'qualified App1bc application-component; old3d object/literalassets context'}
 assert not verify(pins)
 return {**old,'scope':'Unchanged original full3d owning/native87/producer/font/software/Arc authority retained, plus independently qualified App1bc fresh-link+loader COMPONENT. Not whole1bc/0ae/native90/main admission. GUI scenario uses old3d inspector/fixture/font only; NO test runtime DSO/c54 witness.', 'pins':pins,'targets':targets,'original_authority_app_target':original_target,'application_component_scope':declared['scope_tuple'],'component_loader_admission':{'path':str(ADMISSION),'sha256':ADMISSION_SHA},'GUI_runtime_DSO_used':False,'old_source3d_inspector_fixture_font_unchanged':True}
