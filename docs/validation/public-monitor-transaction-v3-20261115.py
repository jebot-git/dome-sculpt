"""Promote observed critical maps only after full expected ELF certification.
The v2 exact-owned-retirement classifier is unchanged; all other errors propagate.
"""
import json,re,time
from pathlib import Path
from monitor_lifetime_v2 import checked_monitor as retirement_checked_monitor

def checked_monitor(monitor,identity,child,expected,owned,critical,critical_stat,seen,mapped,dris,stage,race_log,critical_expectations):
 assert set(critical_expectations)==set(critical), 'Every critical identity needs an expected path and SHA'
 for spec in critical_expectations.values():
  assert Path(spec['path']).is_absolute()and re.fullmatch('[a-f0-9]{64}',spec['sha256'])
 prior_seen=set(seen);trial_seen=set(prior_seen);observed=set(prior_seen)
 def observing_monitor(*args):
  try:return monitor(*args)
  finally:observed.update(trial_seen)
 # The unchanged monitor can swallow ENOENT after adding an inode to trial_seen.
 # The v2 classifier can also return after a proven exact-owned mountinfo EINVAL.
 # Neither outcome can publish an uncertified new critical witness.
 value=retirement_checked_monitor(observing_monitor,identity,child,expected,owned,critical,critical_stat,trial_seen,mapped,dris,stage,race_log)
 promoted=set();withheld=set();certified={}
 for name in observed-prior_seen:
  if name not in critical:continue
  spec=critical_expectations[name];expected_path=str(Path(spec['path']).resolve());matches=[]
  for path,fp in mapped.items():
   if (fp.get('device'),fp.get('inode'))!=critical[name]:continue
   certificate=fp.get('mount_certificate')
   if not certificate:raise RuntimeError(('Incomplete critical ELF certificate',name,path))
   if (fp.get('stat_device'),fp.get('inode'))!=critical_stat[name]or(certificate.get('map_device'),certificate.get('inode'))!=critical[name]or(certificate.get('stat_device'),certificate.get('inode'))!=critical_stat[name]:
    raise RuntimeError(('Critical certificate/device/inode mismatch',name,path))
   if str(Path(path).resolve())!=expected_path or fp.get('sha256')!=spec['sha256']:
    raise RuntimeError(('Critical certified ELF differs from expected path/hash',name,path))
   matches.append({'path':path,'sha256':fp['sha256'],'device':fp['device'],'stat_device':fp['stat_device'],'inode':fp['inode'],'mount_certificate':certificate})
  if matches:promoted.add(name);certified[name]=matches
  else:withheld.add(name)
 # Publish only after every candidate certificate has been checked. On any
 # exception the caller's prior witness set remains exactly unchanged.
 seen.clear();seen.update(prior_seen|promoted)
 if withheld:
  row={'epoch':time.time(),'pid':child.pid,'operation':'transactional critical ELF witness promotion','prior_certified':sorted(prior_seen),'observed':sorted(observed),'promoted':sorted(promoted),'withheld_uncertified':sorted(withheld),'fully_certified_promoted':certified,'identity_after':identity(child.pid),'classification':'Observed map inode without completed matching expected ELF certificate is not a witness; no exception newly excused'}
  with Path(race_log).open('a')as stream:stream.write(json.dumps(row)+'\n')
 return value
