# SPDX-License-Identifier: GPL-3.0-or-later
# Offline audit tool; pass an exact pinned VRM schema tree, with its core refs.
# Requires jsonschema4.26.0; does not alter schema bytes or validated files.
import json,pathlib,struct,sys,hashlib,argparse,importlib.metadata
import jsonschema
from jsonschema import validators
from referencing import Registry,Resource
from referencing.jsonschema import DRAFT4
parser=argparse.ArgumentParser();parser.add_argument('--output',type=pathlib.Path,required=True);parser.add_argument('--spec',type=pathlib.Path,required=True);parser.add_argument('files',type=pathlib.Path,nargs='+');args=parser.parse_args();spec=args.spec;reports=[];selftests=[];schema_hashes={};assert importlib.metadata.version('jsonschema')=='4.26.0'
def unpinned(uri):raise RuntimeError('Unpinned schema reference '+uri)
def mixed_ref(validator,ref,instance,schema):
 resolved=validator._resolver.lookup(ref)
 # Dispatch at each resource boundary. jsonschema4.26's descend closure can
 # otherwise retain Draft4's ignore-ref-siblings behavior for core2020 refs.
 target=resolved.contents;version=target.get('$schema','http://json-schema.org/draft-04/schema')
 if '2020-12'in version:cls=Modern
 elif 'draft-04'in version:cls=Legacy
 else:raise RuntimeError('Unpinned schema dialect '+version)
 yield from cls(target,registry=validator._registry,_resolver=resolved.resolver).iter_errors(instance)
Legacy=validators.extend(jsonschema.Draft4Validator,{'$ref':mixed_ref})
Modern=validators.extend(jsonschema.Draft202012Validator,{'$ref':mixed_ref})
def validator(kind,name):
 directory=spec/kind/'schema';registry=Registry(retrieve=unpinned)
 for file in directory.glob('*.json'):
  digest=hashlib.sha256(file.read_bytes()).hexdigest();assert schema_hashes.setdefault(str(file),digest)==digest;data=json.loads(file.read_text());resource=Resource.from_contents(data,default_specification=DRAFT4);registry=registry.with_resource(file.resolve().as_uri(),resource)
  if '$id'in data:registry=registry.with_resource(data['$id'],resource)
 # Absolute entry reference preserves the declaring retrieval URI for siblings.
 # No pinned schema bytes, constraints or validated instances are rewritten.
 return Legacy({'$ref':(directory/name).resolve().as_uri()},registry=registry)
for path in args.files:
 b=path.read_bytes();j=json.loads(b[20:20+struct.unpack_from('<I',b,12)[0]]);checks=[]
 def validate(kind,name,value):
  errors=[{'path':list(e.absolute_path),'message':e.message}for e in validator(kind,name).iter_errors(value)];checks.append({'kind':kind,'schema':name,'errors':errors})
 for key,kind,name in [('VRMC_vrm','VRMC_vrm-1.0','VRMC_vrm.schema.json'),('VRMC_springBone','VRMC_springBone-1.0','VRMC_springBone.schema.json')]:
  if key in j.get('extensions',{}):validate(kind,name,j['extensions'][key])
 for node in j.get('nodes',[]):
  if 'VRMC_node_constraint'in node.get('extensions',{}):validate('VRMC_node_constraint-1.0','VRMC_node_constraint.schema.json',node['extensions']['VRMC_node_constraint'])
 for material in j.get('materials',[]):
  if 'VRMC_materials_mtoon'in material.get('extensions',{}):
   value=material['extensions']['VRMC_materials_mtoon'];validate('VRMC_materials_mtoon-1.0','VRMC_materials_mtoon.schema.json',value)
   if not selftests:
    role=next((key for key in ['shadingShiftTexture','matcapTexture','rimMultiplyTexture','shadeMultiplyTexture']if key in value),None)
    if role:
     v=validator('VRMC_materials_mtoon-1.0','VRMC_materials_mtoon.schema.json')
     bad=json.loads(json.dumps(value));del bad[role]['index'];assert list(v.iter_errors(bad));selftests.append({'source_file':str(path),'role':role,'control':'Actual missing texture index rejects through pinned core2020 textureInfo siblings'})
     bad=json.loads(json.dumps(value));bad[role]['texCoord']='not an integer';assert list(v.iter_errors(bad));selftests.append({'source_file':str(path),'role':role,'control':'Actual texture coordinate wrong type rejects through pinned core2020 schema'})
     if role=='shadingShiftTexture':
      bad=json.loads(json.dumps(value));bad[role]['scale']='not a number';assert list(v.iter_errors(bad));selftests.append({'source_file':str(path),'role':role,'control':'Actual Shift scale wrong type rejects via pinned Draft4 child schema'})
 reports.append({'file':str(path),'checks':checks})
 print(path,len(checks),sum(len(c['errors'])for c in checks),'schema errors')
assert selftests,'Actual role-bearing input is required for resolver controls'
drift=[p for p,h in schema_hashes.items()if hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()!=h];assert not drift
args.output.write_text(json.dumps({'files':reports,'resolver_controls':selftests,'schema_bytes_rewritten':False,'network_references_allowed':False,'mixed_dialect_dispatch':'Declared Draft4 or2020 perresource; unspecifiedVRM defaultsDraft4','jsonschema_version':'4.26.0','consumed_schema_sha256':schema_hashes,'schema_drift':drift},indent=2)+'\n');raise SystemExit(any(c['errors']for r in reports for c in r['checks']))
