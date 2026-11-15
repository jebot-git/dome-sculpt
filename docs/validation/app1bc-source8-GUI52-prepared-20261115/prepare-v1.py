"""Read-only additive component GUI52 plan. No product/ldd/Xvfb/GUI launched."""
import importlib.util,json,types,hashlib
from pathlib import Path
P=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('component_GUI52_review_driver',P/'run-component-gui-v1.py');driver=importlib.util.module_from_spec(spec);spec.loader.exec_module(driver)
cohort=driver.admit('release');xf=driver.xvfb_contract();args=types.SimpleNamespace(profile='release',phase='gui',case='pbr-quarter',tag='app1bc-component-v1',gui_tag='app1bc-component-v1',review=None,review_sha=None)
plan=driver.plan(args);plan['state']='prepared-NOT-executed';plan['actual_target_details']=cohort['targets'];plan['retained_old_App_target']=cohort['original_authority_app_target'];plan['component_loader_admission']=cohort['component_loader_admission'];plan['old_source3d_inspector_fixture_font_unchanged']=True;plan['GUI_runtime_DSO_used']=False
pins=dict(cohort['pins']);driver.merge_pins(pins,xf['pins']);files={}
for name in['run-component-gui-v1.py','cohort.py','process_observer.py','monitor_lifetime_v2.py','monitor_lifetime_v3.py','scenario-provenance.json','scenarios/source8-public-menu.py','bin/Xvfb',Path(__file__).name]:
 p=P/name;h=driver.sha(p);driver.merge_pins(pins,{str(p):h});files[name]=h
original=driver.R/'artifacts/guard87-validation-admitted-v3-20261115'
assert driver.sha(P/'scenarios/source8-public-menu.py')==driver.sha(original/'scenarios/source8-public-menu.py')
for name in['monitor_lifetime_v2.py','monitor_lifetime_v3.py','process_observer.py']:assert driver.sha(P/name)==driver.sha(original/name)
assert cohort['targets']['app']['sha256']=='4988652c32940b4506fcceb66db6776f0949018972c8bdcc239421845ccdb042'
assert not driver.verify(pins)
plan.update(scope='One unchanged52-check PBR-quarter Source8 GUI regression of App1bc component; old3d inspector/producers/fonts and all old owning/native prerequisites retained. No c54/runtimeDSO use, no new prepared-source GUI coverage, no wholeowning90/main/device/HMD/install acceptance.',old_qualified_scenario_byte_identical=True,old_V3_monitor_and_classifiers_byte_identical=True,existing_correct_RGB_geometry_wrongnormal_bounds=[3,12,7],required_actual_report_checks=52,GUI_step_seconds=50,GUI_phase_seconds=900,GUI_descendant_RSS_bytes=6*2**30,private_Xvfb_health_sha256=driver.XVFB_HEALTH_SHA,no_physical_XR_runtime_env=True,no_product_ldd_in_preparation=True,original_pins_retained=True,new_app_added_at_distinct_path=True,pins=pins,prepared_driver_files=files,GUI_execution_requires_pinned_root_review=True)
for path,name in[(P/'GUI-plan-v1.json','plan'),(P/'cohort-admission-preview-v1.json','cohort')]:
 assert not path.exists();path.write_text(json.dumps(plan if name=='plan'else cohort,indent=2)+'\n')
print(json.dumps({'state':plan['state'],'plan_sha256':driver.sha(P/'GUI-plan-v1.json'),'pins':len(pins),'app':cohort['targets']['app'],'driver_sha256':files['run-component-gui-v1.py'],'cohort_sha256':files['cohort.py'],'scenario_sha256':files['scenarios/source8-public-menu.py'],'unchanged_checks':52,'no_products_executed':True},indent=2))
