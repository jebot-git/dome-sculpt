"""Add whole090f App; retain qualified f1 fixture and6c8 test/runtime roles."""
import importlib.util,json
from pathlib import Path
P=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('prepared_public_f1_components',P/'cohort_v2.py');base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
R,S,B,HEAD,TARGETS,G,sha,write,verify,add=(base.__dict__[k]for k in ['R','S','B','HEAD','TARGETS','G','sha','write','verify','add'])
SOURCE=FIXTURE_SOURCE=base.FIXTURE_SOURCE;SCENARIO_SOURCE=base.SCENARIO_SOURCE
APPLICATION_SOURCE='090f009ede2c0623ca616fe892223b4f68f1c2c4';APP=R/'artifacts/native90-release-planning-20261115/release-090f00-20261117-owning/dome-sculpt';APP_SHA='9143ea5921d35a3612ab452c16eee086403813343a88b3d775196a8507eb8af1'
SOURCE_TUPLE={**base.SOURCE_TUPLE,'application':APPLICATION_SOURCE,'production_body':'1bc13125'}
from runtime_contract import merge_pins

def admit(profile,native_status,native_sha,loader_status,loader_sha):
 prior=base.admit(profile,native_status,native_sha);pins=dict(prior['pins']);add(pins,loader_status,loader_sha);j=json.loads(loader_status.read_text())
 assert j['state']=='terminal'and j['exit']==0 and not j['input_drift']and j['source']==APPLICATION_SOURCE and j['application']=={'path':str(APP),'sha256':APP_SHA}and j['production_body_exact']=='1bc13125'
 assert j['lease']['before_any_ldd_or_product']and j['lease']['released_epoch']>=j['lease']['acquired_epoch']
 assert len(j['steps'])==2 and all(row['exit']==0 and 'error'not in row and not row['signals']for row in j['steps'])
 inputs=loader_status.parent/'inputs-final.json';add(pins,inputs);merge_pins(pins,json.loads(inputs.read_text()));add(pins,APP,APP_SHA)
 rows=j['actual_loader_providers'];add(pins,rows['path'],rows['sha256']);assert rows['count']==72
 for tag in ['native90_admission','whole_owning_admission']:add(pins,j[tag]['path'],j[tag]['sha256'])
 targets={key:dict(row)for key,row in prior['targets'].items()};original_app=dict(targets['app']);assert original_app['path']!=str(APP)
 targets['app']={'path':str(APP),'sha256':APP_SHA,'source_role':'Complete sealed whole090f application, production body1bc; f1 fixture and6c8 runtime/font/scenarios separate'}
 add(pins,P/'cohort_v2.py');add(pins,P/'cohort_v3.py');assert not verify(pins)
 return {**prior,'targets':targets,'pins':pins,'scope':'Whole090f Release application with complete native90 admission, separate f1 producer and unchanged6c8 runtime/font/scenarios. Prior componentApp UI64/XR260 and baseline expectations retained; new whole-App UI is a new actual run.','source_tuple':SOURCE_TUPLE,'original_component_app_preserved':original_app,'whole_application_loader_admission':{'path':str(loader_status),'sha256':loader_sha}}
