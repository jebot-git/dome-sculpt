"""Static artifact readers; never run a project executable or dynamic loader."""
import hashlib,json,re,shlex,stat,subprocess
from pathlib import Path
def sha(path):
 with Path(path).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def identity(path):
 s=Path(path).stat();return {'dev':s.st_dev,'ino':s.st_ino,'size':s.st_size,'mode':stat.S_IMODE(s.st_mode),'mtime_ns':s.st_mtime_ns,'ctime_ns':s.st_ctime_ns}
def ld_dependencies(path):
 # GNU ld follows the actual first rule with -MP-style phony rules; preserve them raw,
 # but do not mistake later rules for dependencies of the actual executable.
 raw=Path(path).read_bytes();lines=raw.decode().replace('\\\n',' ').splitlines();first=next(x for x in lines if x.strip());target,body=first.split(':',1);targets=shlex.split(target);names=shlex.split(body)
 assert len(targets)==1 and names;return {'raw_sha256':hashlib.sha256(raw).hexdigest(),'target':targets[0],'names':names,'other_rules_preserved_raw_count':len([x for x in lines[1:]if x.strip()])}
def compiler_dependencies(path):
 raw=Path(path).read_bytes();target,body=raw.decode().replace('\\\n',' ').split(':',1);names=shlex.split(body);assert names and all(Path(x).is_absolute()for x in names)
 return {'raw_sha256':hashlib.sha256(raw).hexdigest(),'targets':shlex.split(target),'absolute_names':names}
def dynamic_metadata(path,env):
 command=['/usr/bin/readelf','-d',str(path)];r=subprocess.run(command,env=env,capture_output=True,text=True,check=True,timeout=30)
 return {'command':command,'NEEDED':re.findall(r'\(NEEDED\).*?\[(.*?)\]',r.stdout),'RUNPATH':re.findall(r'\(RUNPATH\).*?\[(.*?)\]',r.stdout),'RPATH':re.findall(r'\(RPATH\).*?\[(.*?)\]',r.stdout),'stdout':r.stdout}
def codegen_context(argv,source):
 out=[];i=0
 while i<len(argv):
  token=argv[i]
  if token in['-o','-MF','-MT','-MQ']:i+=2;continue
  if token in['-MD','-MMD','-M','-MM','-c']:i+=1;continue
  if token=='-D':
   assert i+1<len(argv);out.extend(argv[i:i+2]);i+=2;continue
  if token.startswith('-D'):out.append(token);i+=1;continue
  p=Path(token)
  if p.suffix in['.cpp','.cc','.c']and p.is_absolute():i+=1;continue
  out.append(token.replace(str(source),'@SOURCE'));i+=1
 return out
