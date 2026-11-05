#!/usr/bin/env python3
"""Source-only own-header checks; creates no object, archive or executable."""
import hashlib,json,shlex,subprocess,time,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OLD=Path('/home/blux/de-xr/dome-sculpt/artifacts/authoring-next36-cutout-306')
DB=json.loads((OLD/'build-next36-release/compile_commands.json').read_text())
OUT=ROOT/'artifacts/source-audit'/(sys.argv[1] if len(sys.argv)>1 else 'syntax-final');OUT.mkdir(parents=True,exist_ok=False)
SOURCES=['src/document.cpp','src/document-polygon.cpp','src/polygon-authoring.cpp','src/polygon-conversion.cpp','src/polygon-render.cpp','src/app.cpp','src/xr.cpp','tests/polygon-affine.cpp','tests/polygon-document.cpp','tests/polygon-controller.cpp','tests/desktop-framing.cpp','tests/polygon-render.cpp']
def snapshot():
 paths=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard'],cwd=ROOT,text=True).splitlines()
 return {p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths if (ROOT/p).is_file() and (p.startswith(('include/','src/','tests/')) or p=='CMakeLists.txt')}
before=snapshot();(OUT/'before.json').write_text(json.dumps(before,indent=2)+'\n');steps=[]
for source in SOURCES:
 template=next((x for x in DB if x['file'].endswith('/'+source)),None)
 if template is None:template=next(x for x in DB if x['file'].endswith('/src/document.cpp'))
 args=template.get('arguments') or shlex.split(template['command']);cmd=[];skip=False
 for a in args:
  if skip:skip=False;continue
  if a in ('-o','-MF','-MT','-MQ'):skip=True;continue
  if a in ('-c','-MD','-MMD','-MP') or a==template['file']:continue
  cmd.append(a.replace(str(OLD),str(ROOT)))
 cmd+=['-fsyntax-only','-I'+str(ROOT/'src'),'-I'+str(ROOT/'tests'),str(ROOT/source)]
 start=time.monotonic()
 with (OUT/(source.replace('/','-')+'.log')).open('w') as log:r=subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
 steps.append(dict(source=source,command=cmd,exit=r.returncode,seconds=time.monotonic()-start));print(source,r.returncode,flush=True)
 (OUT/'status.json').write_text(json.dumps(dict(state='running',steps=steps),indent=2)+'\n')
 if r.returncode:break
after=snapshot();(OUT/'after.json').write_text(json.dumps(after,indent=2)+'\n');drift=[p for p in sorted(before.keys()|after.keys()) if before.get(p)!=after.get(p)]
ok=len(steps)==len(SOURCES) and not drift and all(s['exit']==0 for s in steps)
(OUT/'status.json').write_text(json.dumps(dict(state='terminal',exit=0 if ok else 1,scope='Syntax-only, no object/archive/executable build or runtime test',steps=steps,drift=drift),indent=2)+'\n')
raise SystemExit(0 if ok else 1)
