"""Retain strict mapping checks while recognizing one proven retired-process read race."""
import errno,json,time
from pathlib import Path

def checked_monitor(monitor,identity,child,expected,owned,critical,critical_stat,seen,mapped,dris,stage,race_log):
 try:
  return monitor(child,owned,critical,critical_stat,seen,mapped,dris,stage)
 except OSError as failure:
  # Linux can return EINVAL opening mountinfo after the exact process loses mm/ns.
  # Never excuse a live process, a reused PID, another path or another error.
  exact_path=str(Path('/proc')/str(child.pid)/'mountinfo')
  if failure.errno!=errno.EINVAL or failure.filename!=exact_path:raise
  observed=owned.get(child.pid)
  if not expected or not observed or observed['start']!=expected['start']:raise
  after=identity(child.pid)
  if after is not None and(after['start']!=expected['start']or after['state']not in ['Z','X']):raise
  row={'epoch':time.time(),'pid':child.pid,'expected_identity':expected,'monitor_owned_identity':observed,'identity_after_failed_read':after,'operation':'open exact owned /proc/PID/mountinfo','errno':failure.errno,'filename':failure.filename,'exception':str(failure),'classification':'same-start retired or absent owned process; no new map certificate or ELF witness admitted','critical_seen_unchanged':sorted(seen),'prior_mapped_ELF_count':len(mapped)}
  with Path(race_log).open('a')as stream:stream.write(json.dumps(row)+'\n')
  return 0
