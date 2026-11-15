"""Read-only identity proof for the explicit private /usr/bin bind in test Xvfb.
Application monitors retain their stricter same-mount-namespace policy unchanged.
"""
import hashlib,importlib.util,json,os
from pathlib import Path,PurePosixPath
P=Path(__file__).resolve().parent;R=P.parents[1];HELPER=R/'artifacts/strict-arc-20261111/control/mount_identity.py'
spec=importlib.util.spec_from_file_location('xvfb_bound_mount_identity',HELPER);cert=importlib.util.module_from_spec(spec);spec.loader.exec_module(cert)
def sha(path):
 with Path(path).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def identity(pid):
 try:
  raw=Path('/proc',str(pid),'stat').read_text();v=raw[raw.rfind(')')+2:].split();return {'start':int(v[19]),'state':v[0]}
 except(FileNotFoundError,ProcessLookupError):return None
def certify(pid,pins,sealing=False):
 before=identity(pid);assert before and before['state']not in ['Z','X']
 exe=Path('/proc',str(pid),'exe');wanted=P/'deps/usr/bin/Xvfb';a=exe.stat();b=wanted.stat();assert(a.st_dev,a.st_ino)==(b.st_dev,b.st_ino)and sha(exe)==pins[str(wanted)]
 ns=os.readlink('/proc/'+str(pid)+'/ns/mnt');maps=Path('/proc',str(pid),'maps').read_text();files={}
 for line in maps.splitlines():
  fields=line.split(None,5)
  if len(fields)!=6 or not fields[5].startswith('/')or fields[5].endswith('(deleted)'):continue
  mapped=Path(fields[5]);probe=Path('/proc',str(pid),'root')/str(mapped).lstrip('/')
  with probe.open('rb')as f:
   if f.read(4)!=b'\x7fELF':continue
  host=(P/'deps/usr/bin'/mapped.relative_to('/usr/bin'))if mapped.is_relative_to('/usr/bin')else mapped;host=host.resolve();st=probe.stat();hs=host.stat();assert(st.st_dev,st.st_ino)==(hs.st_dev,hs.st_ino)
  major,minor=(int(x,16)for x in fields[3].split(':'));map_device=os.makedev(major,minor);inode=int(fields[4]);hostproof=cert.current_identity(host);childmount=cert.covering(pid,mapped)
  assert inode==st.st_ino==hostproof['inode']and map_device==hostproof['map_device']==childmount['mount_device']and st.st_dev==hostproof['stat_device']
  assert childmount['fstype']==hostproof['mount']['fstype']and childmount['source']==hostproof['mount']['source']
  child_fs=PurePosixPath(childmount['root'])/mapped.relative_to(childmount['mountpoint']);host_fs=PurePosixPath(hostproof['mount']['root'])/host.relative_to(hostproof['mount']['mountpoint']);assert child_fs==host_fs,('Different actual bound filesystem path',str(mapped),str(child_fs),str(host_fs))
  digest=sha(probe);assert sha(host)==digest
  if sealing:
   assert str(host)not in pins or pins[str(host)]==digest;pins[str(host)]=digest
  else:assert str(host)in pins and pins[str(host)]==digest,('Actual Xvfb mapped ELF absent/changed from sealed closure',str(host))
  files[str(host)]={'mapped_path':str(mapped),'sha256':digest,'map_device':map_device,'stat_device':st.st_dev,'inode':inode,'host_certificate':hostproof,'child_covering_mount':childmount,'actual_shared_filesystem_path':str(child_fs)}
 after=identity(pid);assert after and after['start']==before['start']and after['state']not in ['Z','X']
 return {'pid':pid,'identity':before,'mount_namespace':ns,'actual_executable_sha256':sha(exe),'maps':maps,'files':files}
