"""Read-only CMake/Ninja graph and ordinary GNU archive inspection."""
from pathlib import Path
import hashlib,re

def sha(path):
 with Path(path).open('rb')as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def logical_lines(text):
 pending=''
 for line in text.splitlines():
  if line.endswith('$'):pending+=line[:-1];continue
  yield pending+line;pending=''
 assert not pending,'Unterminated Ninja continuation'
def tokens(text):
 result=[];token='';i=0
 while i<len(text):
  c=text[i]
  if c=='$'and i+1<len(text)and text[i+1]in ' :$':token+=text[i+1];i+=2;continue
  if c.isspace():
   if token:result.append(token);token=''
  else:token+=c
  i+=1
 if token:result.append(token)
 return result
def graph(build):
 build=Path(build).resolve();edges={};unique=[]
 for line in logical_lines((build/'build.ninja').read_text()):
  if line.startswith('build '):
   match=re.match(r'^build (.*?)(?<!\$): (.*)$',line);assert match,line
   outputs=tokens(match[1]);right=tokens(match[2]);rule=right.pop(0);explicit=[];implicit=[];order=[];current=explicit
   for value in right:
    if value=='|':current=implicit
    elif value in ['||','|@']:current=order
    else:current.append(value)
   edge={'outputs':[p for p in outputs if p!='|'],'rule':rule,'inputs':explicit,'implicit':implicit,'order':order,'variables':{}}
   unique.append(edge)
   for output in edge['outputs']:
    if '$'not in output:edges[output]=edge
  elif line.startswith('  ')and unique:
   match=re.match(r'^  ([A-Za-z0-9_]+) = (.*)$',line)
   if match:unique[-1]['variables'][match[1]]=match[2]
 return edges,unique

def dependency_log(text):
 result={};node=None
 for line in text.splitlines():
  if line and not line.startswith(' '):
   match=re.match(r'^(.*): #deps (\d+), deps mtime (\d+) \((VALID|STALE)\)$',line)
   if not match:raise AssertionError(('Unexpected Ninja deps header',line))
   node=match[1];result[node]={'count':int(match[2]),'mtime':int(match[3]),'state':match[4],'dependencies':[]}
  elif node and line.startswith('    '):result[node]['dependencies'].append(line.strip())
 for name,row in result.items():assert len(row['dependencies'])==row['count'],('Ninja dependency count mismatch',name)
 return result

def archive_members(path):
 records=[];names=b''
 with Path(path).open('rb')as stream:
  assert stream.read(8)==b'!<arch>\n',('Not an ordinary GNU archive',str(path))
  while header:=stream.read(60):
   assert len(header)==60 and header[-2:]==b'`\n'
   raw=header[:16].decode('ascii').strip();size=int(header[48:58]);payload=stream.read(size);assert len(payload)==size
   if size%2:assert stream.read(1)==b'\n'
   if raw=='//':names=payload;continue
   if raw in ['/','/SYM64/']:continue
   if raw.startswith('/'):
    at=int(raw[1:]);end=names.find(b'/\n',at);assert end>=0;name=names[at:end].decode()
   else:assert not raw.startswith('#1/');name=raw.rstrip('/')
   records.append({'name':name,'bytes':size,'sha256':hashlib.sha256(payload).hexdigest()})
 return records
