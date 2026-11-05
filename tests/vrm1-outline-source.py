#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Author a real closed skinned/morphed source cube, preserving source resources.
This is fixture INPUT construction, never an exported-output repair.
"""
from pathlib import Path
import argparse,json,struct,math,copy
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--lit',action='store_true');p.add_argument('--screen',action='store_true');args=p.parse_args();raw=args.source.read_bytes();n=struct.unpack_from('<I',raw,12)[0];j=json.loads(raw[20:20+n]);binary=bytearray(raw[28+n:]);original=bytes(binary)
width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4};formats={5121:'B',5123:'H',5125:'I',5126:'f'}
def values(index):
 a=j['accessors'][index];v=j['bufferViews'][a['bufferView']];fmt='<'+formats[a['componentType']]*width[a['type']];size=struct.calcsize(fmt);offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',size);return [struct.unpack_from(fmt,binary,offset+i*stride)for i in range(a['count'])]
def append(data,kind,component=5126,bounds=False):
 while len(binary)%4:binary.append(0)
 offset=len(binary);fmt='<'+formats[component]*width[kind]
 for row in data:binary.extend(struct.pack(fmt,*row))
 view=len(j['bufferViews']);j['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(binary)-offset});a={'bufferView':view,'componentType':component,'type':kind,'count':len(data)}
 if bounds:a.update(min=[min(x[k]for x in data)for k in range(width[kind])],max=[max(x[k]for x in data)for k in range(width[kind])])
 index=len(j['accessors']);j['accessors'].append(a);return index
faces=[((-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)),((1,-1,-1),(-1,-1,-1),(-1,1,-1),(1,1,-1)),((1,-1,1),(1,-1,-1),(1,1,-1),(1,1,1)),((-1,-1,-1),(-1,-1,1),(-1,1,1),(-1,1,-1)),((-1,1,1),(1,1,1),(1,1,-1),(-1,1,-1)),((-1,-1,-1),(1,-1,-1),(1,-1,1),(-1,-1,1))]
positions=[tuple(v*.1 for v in q)for face in faces for q in face];normals=[tuple(v/math.sqrt(3)for v in q)for face in faces for q in face];uv=[((i%3+(.08 if c in(0,3)else .92))/3,(i//3+(.08 if c in(0,1)else .92))/2)for i in range(6)for c in range(4)];tangents=[]
for face in faces:
 edge=[face[1][k]-face[0][k]for k in range(3)]
 for point in face:
  normal=[v/math.sqrt(3)for v in point];dot=sum(a*b for a,b in zip(edge,normal));t=[edge[k]-normal[k]*dot for k in range(3)];length=math.sqrt(sum(v*v for v in t));tangents.append(tuple(v/length for v in t)+(1,))
for mesh in j['meshes']:
 for primitive in mesh['primitives']:
  old=copy.deepcopy(primitive['attributes']);assert j['accessors'][old['POSITION']]['count']==4
  for name,index in old.items():
   a=j['accessors'][index];data=positions if name=='POSITION'else normals if name=='NORMAL'else tangents if name=='TANGENT'else uv if name.startswith('TEXCOORD_')else values(index)*6;primitive['attributes'][name]=append(data,a['type'],a['componentType'],name=='POSITION')
  primitive['indices']=append([(i*4+c,)for i in range(6)for c in(0,1,2,0,2,3)],'SCALAR',5125)
  for target in primitive.get('targets',[]):
   for name,index in list(target.items()):
    a=j['accessors'][index];target[name]=append(values(index)*6,a['type'],a['componentType'])
for material in j['materials']:
 material.pop('normalTexture',None);material.pop('emissiveTexture',None);material['alphaMode']='OPAQUE';toon=material['extensions']['VRMC_materials_mtoon'];toon.pop('shadeMultiplyTexture',None);material['pbrMetallicRoughness'].pop('baseColorTexture',None);toon.update(outlineWidthMode='screenCoordinates'if args.screen else'worldCoordinates',outlineWidthFactor=.02,outlineColorFactor=[.22,.025,.12],outlineLightingMixFactor=.5 if args.lit else 0)
for expression in j['extensions']['VRMC_vrm']['expressions']['preset'].values():expression.setdefault('materialColorBinds',[]).append({'material':0,'type':'outlineColor','targetValue':[.02,.14,.24,19]})
assert bytes(binary[:len(original)])==original
j['buffers'][0]['byteLength']=len(binary);text=json.dumps(j,separators=(',',':')).encode();text+=b' '*((-len(text))%4);binary+=b'\0'*((-len(binary))%4);out=struct.pack('<III',0x46546c67,2,28+len(text)+len(binary))+struct.pack('<II',len(text),0x4e4f534a)+text+struct.pack('<II',len(binary),0x004e4942)+binary;args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(out)
print(json.dumps({'vertices':24,'triangles':12,'original_binary_prefix_preserved':len(original),'world_unlit':not args.lit and not args.screen,'bytes':len(out)}))
