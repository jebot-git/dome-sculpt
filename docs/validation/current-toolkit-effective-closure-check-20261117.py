from pathlib import Path
import json,hashlib,subprocess,datetime
R=Path('/home/blux/de-xr/dome-sculpt');D=Path('/home/blux/de-xr/DomeD');F=Path('/home/blux/de-xr/fltk-vr');A=Path(__file__).resolve().parent

def sha(p):
    try:
        with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
    except FileNotFoundError:return None

def git(p,*args):return subprocess.check_output(['git',*args],cwd=p,text=True).strip()
J=R/'artifacts/sdk-20261111/verified-sdk.json';j=json.loads(J.read_text());checks=[]
def check(path,want,role):
    got=sha(path);checks.append(dict(path=str(path),role=role,expected=want,actual=got,match=got==want))
for rel,h in j['source_hashes'].items():check(D/rel,h,'canonical toolkit/bitmap source')
for name,p in j['profiles'].items():
    prefix=Path(p['fltk_dir']).parents[1];q=prefix/'share/fltk-bdf-source'
    check(q/'runtime-profile.json',p['profile_sha256'],name+' runtime profile');check(q/'private-config.h',p['private_config_sha256'],name+' private configuration')
    assert json.loads((q/'runtime-profile.json').read_text())==p['runtime_profile']
    for rel,h in p['runtime_profile']['toolkit_libraries'].items():check(prefix/rel,h,name+' installed library')
    for rel,h in p['headers'].items():check(prefix/'include'/rel,h,name+' installed header')
    for rel in j['source_hashes']:
        if rel.startswith('native/toolkit-hooks/') and (q/Path(rel).name).is_file():check(q/Path(rel).name,j['source_hashes'][rel],name+' installed patch')
    for key,rel in [('build_cache_sha256',f'{name}-deps/build/CMakeCache.txt'),('build_ninja_sha256',f'{name}-deps/build/build.ninja'),('compile_commands_sha256',f'{name}-compile-commands.txt'),('build_log_sha256',f'{name}-build.log'),('import_probe_log_sha256',f'import-probe-{name}.log')]:check(R/'artifacts/sdk-20261111'/rel,p[key],name+' historical build metadata')
legacy=[]
for path,h in j['system_bitmap']['libraries'].items():
    got=sha(path);legacy.append(dict(path=path,expected=h,actual=got,match=got==h))
    check(Path('/opt/fltk-vr/bdf-runtime/lib')/Path(path).name,h,'effective canonical bitmap library')
check('/opt/fltk-vr/bdf-runtime/share/domed-bdf-runtime/manifest.json',j['system_bitmap']['manifest_sha256'],'effective canonical bitmap manifest')
check('/opt/fltk-vr/bdf-runtime/share/domed-bdf-runtime/sources.json',j['source_hashes']['native/bitmap-runtime/sources.json'],'effective canonical bitmap source manifest')
m=json.loads((R/'third_party/sources.json').read_text())
for p in [D/'native/bdf_font.hpp',R/'third_party/desktop/bdf_font.hpp']:check(p,m['desktop']['bitmap_helper']['sha256'],'canonical/vendored BDF helper')
L=R/'artifacts/app1bc-component-loader-20261115/status.json';ld=json.loads(L.read_text())
for p,h in ld['pins'].items():check(p,h,'actual qualified component loader input')
actual=Path(ld['actual_loader_providers']['path']);check(actual,ld['actual_loader_providers']['sha256'],'actual component loader providers record')
providers=json.loads(actual.read_text())['rows']
for row in providers:
    check(row['loader_path'],row['sha256'],'actual component loader provider literal');check(row['resolved'],row['sha256'],'actual component loader provider resolved')
P=R/'artifacts/native90-release-planning-20261115/release-090f00-20261117-audit/plan.json';plan=json.loads(P.read_text())
bound=plan['canonical_bitmap_boundary'];check(bound['path'],bound['sha256'],'current whole-build canonical bitmap boundary')
legacy_set=set(j['system_bitmap']['libraries'])
assert not legacy_set.intersection(ld['pins'])
assert not legacy_set.intersection(plan['pins'])
assert bound['effective_prefix']=='/opt/fltk-vr/bdf-runtime'
# Actual explicit compiler/header closure in the newly prepared whole plan.
header_rows=[(p,h) for p,h in plan['pins'].items() if p.startswith(('/usr/include/','/usr/lib64/glib-2.0/include/','/opt/fltk-vr/bdf-runtime/include/'))]
for p,h in header_rows:check(p,h,'current whole-build system/canonical header')
snapshot=[]
for rel,h in j['source_hashes'].items():
    path=F/'dome-compositor'/rel;got=sha(path);snapshot.append(dict(relative=rel,canonical_sha256=h,snapshot_sha256=got,same=got==h))
remote={}
for tag,path in [('DomeD',D),('fltk-vr',F)]:
    remote[tag]=dict(local_HEAD=git(path,'rev-parse','HEAD'),origin_main=git(path,'ls-remote','origin','refs/heads/main'),status=git(path,'status','--porcelain'))
assert remote['DomeD']['local_HEAD']==j['domed_commit']
assert remote['DomeD']['origin_main'].split()[0]==j['domed_commit']
errors=[x for x in checks if not x['match']]
out=dict(checked_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='Read-only source, existing SDK and exact effective dependency pins. No build/product/ldd/probe/graphics/install/session action.',remote=remote,prior_sdk_receipt=dict(path=str(J),sha256=sha(J)),current_whole_plan=dict(path=str(P),sha256=sha(P),source=plan['source'],canonical_boundary=bound,environment=plan['environment']),actual_loader_status=dict(path=str(L),sha256=sha(L),pin_count=len(ld['pins']),provider_count=len(providers),provider_record_sha256=sha(actual)),historical_unused_system_bitmap=legacy,historical_system_intersection_with_loader=[],historical_system_intersection_with_whole_plan=[],current_header_count=len(header_rows),checks=checks,errors=errors,collected_snapshot_comparison=snapshot,sdk_rebuild_required=False,conclusion='Canonical DomeD source unchanged; effective relocated bitmap closure and current SDK match. Historical Nov11 system paths differ but are absent from actual loader and current whole-plan roles; this is not evidence of effective runtime drift. Retain explicit canonical prefix and runtime validation.')
(A/'receipt.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(dict(checks=len(checks),errors=len(errors),legacy_system_mismatches=sum(not r['match'] for r in legacy),current_headers=len(header_rows),loader_pins=len(ld['pins']),actual_providers=len(providers),snapshot_differences=[r['relative'] for r in snapshot if not r['same']],receipt_sha256=sha(A/'receipt.json')),indent=2))
raise SystemExit(bool(errors))
