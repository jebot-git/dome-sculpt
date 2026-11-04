#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Test-only genuinely unskinned derivative; retained accessor element bytes exact.
import sys
import json,pathlib,struct,hashlib,copy
root=pathlib.Path(__file__).resolve().parents[1];dest=pathlib.Path(sys.argv[1]);dest.mkdir(parents=True,exist_ok=True)
records=[]
for sex in ['female','male']:
 original=(root/'assets/humanoid/vroid'/('base-'+sex+'.vrm')).read_bytes()
 magic,version,length=struct.unpack_from('<III',original);assert magic==0x46546c67 and version==2 and length==len(original)
 chunks=[];at=12
 while at<len(original):
  n,kind=struct.unpack_from('<II',original,at);chunks.append((kind,original[at+8:at+8+n]));at+=8+n
 data=json.loads(next(v for k,v in chunks if k==0x4e4f534a));binary=next(v for k,v in chunks if k==0x004e4942)
 static=copy.deepcopy(data);old_views=data['bufferViews'];old_accessors=data['accessors'];new_views=[];new_accessors=[];new_bin=bytearray();accessor_map={};kept=[]
 def put(payload,target=None):
  new_bin.extend(b'\0'*((-len(new_bin))%4));offset=len(new_bin);new_bin.extend(payload)
  view={'buffer':0,'byteOffset':offset,'byteLength':len(payload)}
  if target: view['target']=target
  new_views.append(view);return len(new_views)-1
 def accessor(index):
  if index in accessor_map:return accessor_map[index]
  a=copy.deepcopy(old_accessors[index]);assert 'sparse' not in a and 'bufferView' in a
  v=old_views[a['bufferView']];assert v.get('buffer',0)==0
  width={5120:1,5121:1,5122:2,5123:2,5125:4,5126:4}[a['componentType']]*{'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
  start=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',width)
  payload=b''.join(binary[start+i*stride:start+i*stride+width] for i in range(a['count']))
  assert len(payload)==width*a['count']
  a['bufferView']=put(payload,v.get('target'));a.pop('byteOffset',None)
  accessor_map[index]=len(new_accessors);new_accessors.append(a)
  kept.append({'original_accessor':index,'count':a['count'],'type':a['type'],'sha256':hashlib.sha256(payload).hexdigest()})
  return accessor_map[index]
 removed_attributes=0
 for mesh in static['meshes']:
  for p in mesh['primitives']:
   assert not p.get('extensions',{}).get('KHR_draco_mesh_compression')
   for name in list(p['attributes']):
    if name.startswith('JOINTS_') or name.startswith('WEIGHTS_'):del p['attributes'][name];removed_attributes+=1
    else:p['attributes'][name]=accessor(p['attributes'][name])
   if 'indices' in p:p['indices']=accessor(p['indices'])
   for target in p.get('targets',[]):
    for name,index in list(target.items()):target[name]=accessor(index)
 for image in static.get('images',[]):
  assert 'bufferView' in image and 'uri' not in image
  v=old_views[image['bufferView']];start=v.get('byteOffset',0);payload=binary[start:start+v['byteLength']]
  image['bufferView']=put(payload)
 for node in static['nodes']:node.pop('skin',None)
 static.pop('skins',None);static.pop('animations',None)
 if 'extensions' in static:static['extensions'].pop('VRM',None)
 for key in ['extensionsUsed','extensionsRequired']:
  if key in static:static[key]=[v for v in static[key] if v!='VRM']
 static['accessors']=new_accessors;static['bufferViews']=new_views;static['buffers']=[{'byteLength':len(new_bin)}]
 static.setdefault('extras',{})['fixture_provenance']={'original_cc0_sha256':hashlib.sha256(original).hexdigest(),'operation':'Static unskinned derivative; skin/joint/weight/IBM/animation payloads excluded, all other authored accessor elements retained byte-exact.'}
 assert all(not any(k.startswith(('JOINTS_','WEIGHTS_')) for k in p['attributes']) for m in static['meshes'] for p in m['primitives'])
 text=json.dumps(static,separators=(',',':'),ensure_ascii=False).encode();text+=b' '*((-len(text))%4);new_bin.extend(b'\0'*((-len(new_bin))%4))
 output=struct.pack('<III',magic,version,12+8+len(text)+8+len(new_bin))+struct.pack('<II',len(text),0x4e4f534a)+text+struct.pack('<II',len(new_bin),0x004e4942)+new_bin
 path=dest/(sex+'-static.glb');path.write_bytes(output)
 records.append({'sex':sex,'original_sha256':hashlib.sha256(original).hexdigest(),'static_sha256':hashlib.sha256(output).hexdigest(),'mesh_count':len(static['meshes']),'primitive_count':sum(len(m['primitives']) for m in static['meshes']),'removed_skin_attributes':removed_attributes,'retained_accessors':kept,'new_binary_bytes':len(new_bin)})
 print(sex,'static_bytes',len(output),'accessors',len(new_accessors),'removed_skin_attributes',removed_attributes)
(dest/'manifest.json').write_text(json.dumps(records,indent=2)+'\n')
