"""Fresh strictSAN current-toolchain owning plan. No provider objects/products reused."""
import os,json,subprocess,shlex,hashlib,re,shutil
from pathlib import Path
from ninja_audit import sha,graph
P=Path(__file__).resolve().parent;R=P.parents[1];SOURCE=R/'artifacts/authoring-paused-uv-domain-20261112';HEAD='3d1643827dd56e1b2ef1d57b8a9c6ff1e4211a2c';SDK=R/'artifacts/sdk-20261111/toolkit-sanitize';BITMAP=Path('/opt/fltk-vr/bdf-runtime');DEST=P/'sanitize-3d1643-current-fresh-v5-owning';AUDIT=P/'sanitize-3d1643-current-fresh-v5-audit';PINS={}
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n')
def pin(path):
 p=Path(path);assert p.is_file(),('Missing current input',str(p));value=sha(p)
 assert str(p)not in PINS or PINS[str(p)]==value,('Changed current input',str(p));PINS[str(p)]=value;return value
def canonical(value):
 value=value.replace('${cmake_ninja_workdir}',str(DEST)+'/');p=Path(value)
 return str(p.relative_to(DEST))if p.is_absolute()and p.is_relative_to(DEST)else value
def commands(outputs):
 out=subprocess.check_output(['ninja','-C',str(DEST),'-t','commands','-s',*outputs],text=True).splitlines();assert len(out)==len(outputs);return dict(zip(outputs,out))
def current_tools(cache):
 tools={}
 def add(value,role):
  p=Path(value);p=p if p.is_absolute()else Path(shutil.which(value)or value);assert p.is_file(),(value,role);pin(p);pin(p.resolve());tools.setdefault(str(p),{'resolved':str(p.resolve()),'sha256':sha(p),'roles':[]})['roles'].append(role);return p
 for key in ['CMAKE_C_COMPILER','CMAKE_CXX_COMPILER','CMAKE_AR','CMAKE_RANLIB','CMAKE_LINKER','CMAKE_MAKE_PROGRAM']:add(cache[key],key)
 cxx=Path(cache['CMAKE_CXX_COMPILER'])
 for value in ['cc1plus','cc1','collect2','ld','as']:add(subprocess.check_output([str(cxx),'-print-prog-name='+value],text=True).strip(),'actual compiler program '+value)
 for value in ['libasan.so','libubsan.so','libstdc++.so','libgcc_s.so']:add(subprocess.check_output([str(cxx),'-print-file-name='+value],text=True).strip(),'actual compiler runtime '+value)
 return tools
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=SOURCE,text=True).strip()==HEAD and not subprocess.check_output(['git','status','--porcelain'],cwd=SOURCE,text=True)
AUDIT.mkdir();DEST.mkdir()
source_files=subprocess.check_output(['git','ls-files','-z'],cwd=SOURCE,text=True).split('\0');source_pins={str(SOURCE/x):pin(SOURCE/x)for x in source_files if x}
relocation=P/'canonical-sdk-relocation-v1.json';rel=json.loads(relocation.read_text());pin(relocation)
for key in ['historical_sdk_manifest','canonical_manifest','canonical_checker','check_log']:
 row=rel[key];assert pin(row['path'])==row['sha256']
for path,value in rel['effective_current_sdk_pins'].items():assert pin(path)==value
flags='-O1 -g1 -DNDEBUG -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer';ldflags='-Wl,--gc-sections -fsanitize=address,undefined';env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
for key in ['ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LD_PRELOAD']:env.pop(key,None)
env.update(PKG_CONFIG_PATH=str(BITMAP/'lib/pkgconfig')+':/usr/lib64/pkgconfig',LD_LIBRARY_PATH=str(SDK/'lib')+':'+str(BITMAP/'lib')+':/usr/lib64',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
configure=['cmake','-G','Ninja','-S',str(SOURCE),'-B',str(DEST),'-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=ON','-DDOME_BUILD_UI=ON','-DDOME_BUILD_XR=ON','-DDOME_COMPUTE_BACKEND=CPU','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DFLTK_DIR='+str(SDK/'share/fltk'),'-DCMAKE_CXX_FLAGS_RELEASE='+flags,'-DCMAKE_C_FLAGS_RELEASE='+flags,'-DCMAKE_EXE_LINKER_FLAGS='+ldflags]
with(AUDIT/'configure.log').open('w')as f:subprocess.run(configure,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
assert not list(DEST.rglob('*.o'))
cache={}
for line in(DEST/'CMakeCache.txt').read_text().splitlines():
 if not line.startswith(('#','//'))and ':'in line and '='in line:key,value=line.split('=',1);cache[key.split(':',1)[0]]=value
assert cache['CMAKE_C_FLAGS_RELEASE']==flags and cache['CMAKE_CXX_FLAGS_RELEASE']==flags and cache['CMAKE_EXE_LINKER_FLAGS']==ldflags and cache['DOME_COMPUTE_BACKEND']=='CPU'and cache['DOME_BUILD_UI']=='ON'and cache['DOME_BUILD_XR']=='ON'and cache['FLTK_DIR']==str(SDK/'share/fltk')
tools=current_tools(cache);nodes,unique=graph(DEST);nodes={canonical(k):v for k,v in nodes.items()};cc=json.loads((DEST/'compile_commands.json').read_text());objects={}
objnames=[str(Path(row['output']).relative_to(DEST))for row in cc];actual_compile=commands(objnames)
for key,row in zip(objnames,cc):
 argv=shlex.split(actual_compile[key]);assert argv[0]==cache['CMAKE_CXX_COMPILER']or argv[0]==cache['CMAKE_C_COMPILER'],('Nontrivial compiler shell rule',key,argv[:3]);assert '-MD'in argv and '-MF'in argv and '-MT'in argv;assert argv[argv.index('-o')+1]==key
 objects[key]={'source':row['file'],'command':argv,'route':'compile-complete-TU','reasons':['Fresh current-toolchain cohort; historical compiler identity changed, zero reuse'],'provider':None,'provider_sha256':None,'depfile':argv[argv.index('-MF')+1]}
LINK_RULES=('STATIC_LIBRARY_LINKER','EXECUTABLE_LINKER','SHARED_LIBRARY_LINKER','MODULE_LIBRARY_LINKER');link_edges={canonical(e['outputs'][0]):e for e in unique if any(x in e['rule']for x in LINK_RULES)};actual_link=commands(list(link_edges));links={}
for key,e in link_edges.items():links[key]={'route':'fresh-current-link','command':actual_link[key],'dependencies':[canonical(x)for x in e['inputs']+e['implicit']if canonical(x)in link_edges],'object_inputs':[canonical(x)for x in e['inputs']if canonical(x)in objects],'reasons':['Fresh current-toolchain cohort; no whole-product reuse'],'provider':None,'provider_sha256':None,'archive':key.endswith('.a')}
tests={}
for line in(DEST/'CTestTestfile.cmake').read_text().splitlines():
 m=re.match(r'^add_test\(\[=\[(.*?)\]=\] (.*)\)$',line)
 if m:tests[m[1]]=shlex.split(m[2])
manifest=R/'artifacts/authoring-source8-visible-float32-configure-20261112/status.json';pin(manifest);selected=json.loads(manifest.read_text())['required_native_test_names']+['humanoid-component-bind','source-role-domain-emission','source-role-missing-uv1'];assert len(selected)==len(set(selected))==87 and set(selected)<=set(tests)
required={'dome-sculpt','dome-sculpt-baked-emission-workflow-test','dome-sculpt-source8-public-fixture','dome-sculpt-baked-emission-font-reference','libdome-sculpt-baked-emission-runtime.so','libdome-sculpt-source8-public-runtime.so','dome-sculpt-sculpt-chunk-workspace-test','dome-sculpt-sculpt-placed-workspace-test','dome-sculpt-texture-set-source-render-test'}|{str(Path(tests[x][0]).relative_to(DEST))for x in selected};assert required<=set(links)
reachable=set();todo=list(objects)+list(links);generators={}
while todo:
 key=canonical(todo.pop())
 if key in reachable:continue
 reachable.add(key);e=nodes.get(key)
 if not e:continue
 if e['rule']=='CUSTOM_COMMAND':generators[canonical(e['outputs'][0])]=e
 todo+=e['inputs']+e['implicit']+e['order']
generator_commands=commands(list(generators));oldplan=R/'artifacts/source8-final-owning-sanitize-plan-20261112/sanitize-5c820f-v1-audit/plan-with-external-freshness-v2.json';pin(oldplan);old=json.loads(oldplan.read_text())
for key,e in list(generators.items()):
 records=[]
 for value in e['inputs']+e['implicit']:
  f=Path(value);f=f if f.is_absolute()else DEST/f;value_sha=pin(f);records.append({'current':str(f),'sha256':value_sha})
  if f.is_relative_to(SOURCE):assert any(x['sha256']==value_sha for x in old['generated_outputs'][key]['inputs']),('Generator source changed',key,str(f))
 command=generator_commands[key]
 for token in shlex.split(command):
  f=Path(token)
  if f.is_absolute()and f.is_file()and os.access(f,os.X_OK):pin(f);pin(f.resolve())
 generators[key]={'route':'run-exact-current-generator','command':command,'inputs':records,'expected_sha256':old['generated_outputs'][key]['expected_sha256'],'comparison_scope':'Only expected generated bytes from preserved source-identical generator inputs; actual command runs freshly. No object/product staging.'}
material=set(objects)|set(links)|set(generators)
def prereqs(key):
 e=nodes[key];todo=e['inputs']+e['implicit']+e['order'];seen=set();out=set()
 while todo:
  name=canonical(todo.pop())
  if name in seen:continue
  seen.add(name)
  if name in material:out.add(name);continue
  edge=nodes.get(name)
  if edge:todo+=edge['inputs']+edge['implicit']+edge['order'];continue
  f=Path(name);f=f if f.is_absolute()else DEST/f
  if name=='.'and f.is_dir():continue
  pin(f)
 assert key not in out;return sorted(out)
for group in [objects,links,generators]:
 for key,row in group.items():row['dag_dependencies']=prereqs(key)
finished=set();pending={k:v for group in[objects,links,generators]for k,v in group.items()};layers=[]
while pending:
 ready=sorted(k for k,v in pending.items()if set(v['dag_dependencies'])<=finished);assert ready,('Unclosed fresh owning DAG',list(pending)[:5]);layers.append(ready);finished.update(ready)
 for key in ready:del pending[key]
# Pin current actual configured header-search roots before any whole-TU work.
roots=set();include_queries={}
for language,compiler in [('c',cache['CMAKE_C_COMPILER']),('c++',cache['CMAKE_CXX_COMPILER'])]:
 out=subprocess.run([compiler,'-E','-x',language,'-v','/dev/null','-o','/dev/null'],env=env,capture_output=True,text=True,check=True,timeout=30);include_queries[language]=out.stderr;inside=False
 for line in out.stderr.splitlines():
  if '#include <...> search starts here:'in line:inside=True;continue
  if 'End of search list.'in line:inside=False
  if inside:
   f=Path(line.strip());assert f.is_dir(),line;roots.add(f)
for row in objects.values():
 a=row['command'];i=0
 while i<len(a):
  token=a[i]
  if token in ['-I','-isystem','-iquote']:
   i+=1;roots.add(Path(a[i]))
  elif token.startswith('-I')and len(token)>2:roots.add(Path(token[2:]))
  i+=1
header_records={};visited_header_dirs=set()
for root in sorted(roots):
 root=root if root.is_absolute()else DEST/root
 if root.is_relative_to(SOURCE)or root.is_relative_to(DEST):continue
 assert root.is_dir(),('Missing actual include search root',root)
 for current,dirs,files in os.walk(root,followlinks=True):
  physical=str(Path(current).resolve())
  if physical in visited_header_dirs:dirs[:]=[];continue
  visited_header_dirs.add(physical)
  for name in files:
   f=Path(current)/name
   if f.is_file():header_records[str(f)]=pin(f)
# Actual current configured ELF library/tool dependency forecast; runtime mapping
# remains a later acceptance gate, not an inference from this ldd census.
elf_candidates={Path(x)for x in PINS if Path(x).suffix.startswith('.so')or os.access(x,os.X_OK)}|{Path(x)for x in tools}
for root in [SDK/'lib',BITMAP/'lib']:
 elf_candidates.update(x for x in root.glob('*.so*')if x.is_file())
elf_closure={}
for f in sorted(elf_candidates):
 if not f.is_file():continue
 with f.open('rb')as stream:elf_magic=stream.read(4)
 if elf_magic!=b'\x7fELF':continue
 proc=subprocess.run(['ldd',str(f)],env=env,capture_output=True,text=True,timeout=30)
 assert 'not found'not in proc.stdout,('Current ELF dependency unavailable',str(f),proc.stdout)
 paths=re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)',proc.stdout,re.M)
 for value in paths:
  q=Path(value);elf_closure[str(q)]=pin(q);pin(q.resolve())
for f in [DEST/'compile_commands.json',DEST/'build.ninja',DEST/'CMakeFiles/rules.ninja',DEST/'CMakeCache.txt',DEST/'CTestTestfile.cmake',Path(__file__),P/'ninja_audit.py',P/'execute_sanitize-fresh-v5.py',P/'dependency_audit_v5.py']:
 pin(f)
for f in(DEST/'CMakeFiles').glob('*/CMake*CCompiler.cmake'):pin(f)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=SOURCE,text=True).strip()==HEAD and not subprocess.check_output(['git','status','--porcelain'],cwd=SOURCE,text=True)
drift=[x for x,h in PINS.items()if not Path(x).is_file()or sha(x)!=h];assert not drift,('Current initial/final plan drift',drift[:8])
summary={'configured_TUs':len(objects),'fresh_complete_TUs':len(objects),'reused_whole_objects':0,'all_configured_link_archive_nodes':len(links),'fresh_links_archives':len(links),'retained_whole_products':0,'mandatory_fresh_owning_targets':len(required),'selected_CTests':87,'additional_manual_source_render_gate':1}
assert len(objects)==1032 and len(links)==733 and len(generators)==3,(summary,len(generators))
result={'state':'audited-NOT-compiled','source':HEAD,'source_path':str(SOURCE),'profile':'sanitize','destination':str(DEST),'scope':'Fresh entire current-toolchain configured whole-TU/archive/link closure, zero object/product reuse. Actual Ninja compiler commands emit genuine depfiles. Historical source/providers/failed audits untouched.','summary':summary,'objects':objects,'links':links,'generated_outputs':generators,'pins':PINS,'selected_CTests':selected,'selected_commands':{x:tests[x]for x in selected},'required_targets':sorted(required),'generated_dependency_audit':{'topological_layers':layers,'reachable_ninja_nodes':len(reachable),'material_nodes':len(material)},'source_tree':source_pins,'current_toolchain':{'flags':flags,'link_flags':ldflags,'C_FLAGS_RELEASE':cache['CMAKE_C_FLAGS_RELEASE'],'C_instrumentation_scope':'New current fresh cohort explicitly instruments every C owner as well as C++; historical CFLAGS default-O3 scope remains unchanged in old receipts.','tools':tools,'headers':{'search_roots':[str(x)for x in sorted(roots)],'pinned_files':len(header_records),'compiler_queries':include_queries},'ELF_forecast':{'pins':elf_closure,'scope':'Current ldd dependency availability forecast only; actual mapped process/device evidence required at runtime.'}},'canonical_bitmap_boundary':{'path':str(relocation),'sha256':sha(relocation),'effective_prefix':str(BITMAP),'SDK':str(SDK)},'environment':{x:env.get(x)for x in ['PKG_CONFIG_PATH','LD_LIBRARY_PATH','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LD_PRELOAD']},'resource_bounds':{'independent_owning_seconds':18000,'max_active_tasks':2,'aggregate_descendant_RSS_bytes':8*2**30,'initial_input_audit_seconds':600,'final_input_audit_seconds':600},'initial_objects':0,'whole_provider_stage_count':0,'compile_link_authorization_required_before_launch':True,'runtime_policy':'No native/software/graphics product before complete materialization0 and parent coordination; native test timeouts unchanged. No install/HMD/session restart.'}
write(AUDIT/'plan.json',result);print(json.dumps(summary,indent=2));print('GENERATORS',len(generators),'PINS',len(PINS),'HEADERS',len(header_records),'PLAN_SHA256',sha(AUDIT/'plan.json'))
