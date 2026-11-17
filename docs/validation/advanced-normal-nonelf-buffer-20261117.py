"""Certify only live software-renderer shared lp_dma_buf allocations, never ELF."""
import os,stat
from pathlib import Path
NAME='/memfd:lp_dma_buf (deleted)'
def identity(pid):
 try:
  text=Path('/proc',str(pid),'stat').read_text();v=text[text.rfind(')')+2:].split()
  return {'pid':pid,'start':int(v[19]),'pgrp':int(v[2]),'session':int(v[3]),'state':v[0]}
 except(FileNotFoundError,ProcessLookupError):return None
def same(root,now):return bool(now and now['state']!='Z'and all(now[k]==root[k]for k in ['pid','start','pgrp','session']))
def eligible(row):
 fields=row.split(None,5)
 return len(fields)==6 and fields[5]==NAME and fields[1]=='rw-s'and int(fields[2],16)==0 and int(fields[4])>0

def certify(root,row):
 assert eligible(row),'Buffer exception is restricted to exact non-executable offset-zero shared allocation'
 pid=root['pid'];before=identity(pid);assert same(root,before),'Original live owner identity changed'
 maps=Path('/proc',str(pid),'maps');assert row in maps.read_text().splitlines(),'Allocation no longer mapped by proven live process'
 fields=row.split(None,5);address=int(fields[0].split('-')[0],16);major,minor=(int(v,16)for v in fields[3].split(':'));inode=int(fields[4]);evidence=None
 for fd in Path('/proc',str(pid),'fd').iterdir():
  try:
   if os.readlink(fd)!=NAME:continue
   handle=os.open(fd,os.O_RDONLY|os.O_CLOEXEC)
   try:
    info=os.fstat(handle)
    if not stat.S_ISREG(info.st_mode)or info.st_dev!=os.makedev(major,minor)or info.st_ino!=inode:continue
    magic=os.pread(handle,4,0)
    if len(magic)!=4:continue
    evidence={'method':'matching-live-child-FD','FD':str(fd),'stat_device':info.st_dev,'stat_inode':info.st_ino,'stat_mode':info.st_mode,'first_four_hex':magic.hex()}
   finally:os.close(handle)
   if evidence:break
  except OSError:continue
 if evidence is None:
  # Read only four bytes from the exact mapped address in our proven child;
  # never inspect arbitrary processes or persist the buffer's other contents.
  handle=os.open(Path('/proc',str(pid),'mem'),os.O_RDONLY|os.O_CLOEXEC)
  try:magic=os.pread(handle,4,address)
  finally:os.close(handle)
  assert len(magic)==4,'Cannot prove readable non-ELF allocation magic'
  evidence={'method':'matching-live-child-map-memory','address':address,'first_four_hex':magic.hex()}
 assert evidence['first_four_hex']!='7f454c46','Deleted ELF mapping must never be exempted'
 assert row in maps.read_text().splitlines(),'Allocation changed during certification'
 after=identity(pid);assert same(root,after)and same(before,after),'Live owner identity changed during certification'
 return {'row':row,'owner_before':before,'owner_after':after,'permissions':fields[1],'offset':0,'device_major':major,'device_minor':minor,'inode':inode,'exact_allocation_name':NAME,'non_executable':True,'proven_non_ELF':True,**evidence}
