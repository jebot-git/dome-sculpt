import pathlib,struct,json,collections,hashlib
records=[]
for sex in ['female','male']:
 path=pathlib.Path('../automatic-weights-native-context-api-7849001/build/native-static')/(sex+'-static.glb');b=path.read_bytes();n=struct.unpack_from('<I',b,12)[0];j=json.loads(b[20:20+n]);data=b[28+n:];m=j['meshes'][0];rows=[];parts=[]
 assert not j.get('skins')
 def read(index):
  a=j['accessors'][index];v=j['bufferViews'][a['bufferView']];count=a['count'];o=v.get('byteOffset',0)+a.get('byteOffset',0);assert a['componentType']==5126 and a['type']=='VEC3';stride=v.get('byteStride',12);return [data[o+i*stride:o+i*stride+12]for i in range(count)]
 for p in m['primitives']:
  a=j['accessors'][p['indices']];v=j['bufferViews'][a['bufferView']];fmt={5121:'B',5123:'H',5125:'I'}[a['componentType']];w=struct.calcsize(fmt);indices=[struct.unpack_from('<'+fmt,data,v.get('byteOffset',0)+a.get('byteOffset',0)+i*w)[0]for i in range(a['count'])];used=set(indices)
  pos=read(p['attributes']['POSITION']);targets=[read(t['POSITION']) if 'POSITION' in t else [bytes(12)]*len(pos)for t in p.get('targets',[])]
  signatures={v:b''.join(t[v]for t in targets)for v in used}
  # zero signs canonicalized only for diagnostic exact-value signatures.
  signatures={v:struct.pack('<'+'f'*(len(k)//4),*(0. if x==0 else x for x in struct.unpack('<'+'f'*(len(k)//4),k)))for v,k in signatures.items()}
  edges=collections.defaultdict(list)
  for c in range(0,len(indices),3):
   for k in range(3):
    u,z=indices[c+k],indices[c+(k+1)%3];edges[tuple(sorted((u,z)))].append((u,z))
  boundary=[es[0]for es in edges.values()if len(es)==1]
  parts.append((used,pos,signatures,boundary))
 for a in range(len(parts)):
  for c in range(a+1,len(parts)):
   ua,pa,sa,ba=parts[a];uc,pc,sc,bc=parts[c];other=collections.defaultdict(list)
   for v in uc:
    if any(sc[v]):other[sc[v]].append(v)
   matches=0;exactpos=0;mindist=float('inf');pairs=[]
   for v in ua:
    if sa[v]not in other:continue
    x=struct.unpack('<fff',pa[v])
    ds=[]
    for z in other[sa[v]]:
     y=struct.unpack('<fff',pc[z]);ds.append((sum((x[k]-y[k])**2 for k in range(3)),z))
    d,z=min(ds);mindist=min(mindist,d**.5);matches+=1;exactpos+=d==0;pairs.append((v,z,d**.5))
   if matches:rows.append({'parts':[a,c],'matching_nonzero_morph_rows':matches,'exact_position_rows':exactpos,'minimum_distance':mindist,'pairs':pairs})
 groups=collections.defaultdict(list)
 for owner,(used,pos,sigs,boundary) in enumerate(parts):
  for u,v in boundary:
   keys=[pos[z]+sigs[z]for z in [u,v]]
   groups[tuple(sorted(keys))].append((owner,u,v,keys))
 seam_edges=collections.Counter();ambiguous=0
 for key,edges in groups.items():
  if len(edges)==1:continue
  if len(edges)!=2:ambiguous+=1;continue
  a,c=edges
  if a[0]==c[0] or a[3][0]!=c[3][1] or a[3][1]!=c[3][0]:ambiguous+=1;continue
  seam_edges[tuple(sorted((a[0],c[0])))]+=1
 records.append({'oppositely_oriented_unique_boundary_edges':[{'parts':pair,'count':count}for pair,count in seam_edges.items()], 'ambiguous_boundary_groups':ambiguous,'sex':sex,'source_sha256':hashlib.sha256(b).hexdigest(),'same_original_mesh_only':True,'all_authored_targets_compared':True,'pairs':rows})
pathlib.Path('artifacts/morph-contact-audit.json').write_text(json.dumps(records,indent=2))
for r in records:
 print(r['sex']);print([{k:v for k,v in x.items()if k!='pairs'}for x in r['pairs']])
