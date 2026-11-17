"""Pure read-only route policy for sealed CURRENT-toolchain whole providers.
Callers must first verify terminal provider status, whole outputs, current tools,
all original/current pins and genuine actual compiler dependency manifest. This
module never treats an old compiler/Ninja snapshot as eligibility evidence.
"""
from pathlib import Path
import hashlib
FUNCTIONAL_SOURCE_PATHS=frozenset({'third_party/desktop/bdf_font.hpp','src/vroid-creator.cpp','tests/layered-normal-native.cpp','tests/source-rest-ui-fixture.cpp'})
def sha_file(path):
 h=hashlib.sha256()
 with Path(path).open('rb')as f:
  while block:=f.read(1024*1024):h.update(block)
 return h.hexdigest()
def normalize_actual_command(argv,source,build):
 """Only comparison context; NEVER rewrite or normalize a literal -D token."""
 source=str(Path(source));build=str(Path(build));out=[];index=0
 while index<len(argv):
  token=argv[index]
  if token=='-D':
   assert index+1<len(argv), 'Standalone -D has no literal operand'
   out.extend([token,argv[index+1]]);index+=2;continue
  out.append(token if token.startswith('-D')else token.replace(source,'@SOURCE').replace(build,'@BUILD'));index+=1
 return out

def new_header_basenames(old_tree,new_tree):
 return {Path(k).name for k in new_tree if k not in old_tree and Path(k).suffix in {'.h','.hpp','.hh','.hxx','.inc'}}
def _relative(path,root):
 try:return str(Path(path).relative_to(root))
 except ValueError:return None

def decide_current_whole_object(*,key,new_command,provider_command,actual_dependency_record,old_source,new_source,old_build,new_build,new_tree,provider_path,provider_sha256,current_assets_identical,generated_aliases,current_generated_expected,new_headers=frozenset(),source_sensitive_paths=FUNCTIONAL_SOURCE_PATHS,hash_file=sha_file):
 """Return explicit reasons; an empty list permits whole-byte provider reuse.
 generated_aliases maps each genuine prior build-produced physical dependency to
 {'key': current generator node key, 'sha256': sealed prior expected bytes}.
 It must be derived from actual current provider/ancestor ownership receipts,
 never guessed from a header basename. New generated expected bytes require
 source/tool proof separately. new_tree maps genuine current tracked relative
 source paths to observed SHA256. Broader caller __FILE__/source_location policy
 is accepted; the four known functional paths are always mandatory.
 """
 old_source=Path(old_source).resolve();new_source=Path(new_source).resolve();old_build=Path(old_build).resolve();new_build=Path(new_build).resolve();why=[];checked=[]
 if provider_command is None:why.append('New configured complete TU has no current provider command')
 elif normalize_actual_command(provider_command,old_source,old_build)!=normalize_actual_command(new_command,new_source,new_build):why.append('Actual compiler/options/literal defines differ; complete TU required')
 if not current_assets_identical:why.append('Effective asset domain differs; source-prefix equivalence unproved')
 if actual_dependency_record is None:why.append('No genuine sealed CURRENT compiler dependency record')
 source_changed=old_source!=new_source;sensitive=set(source_sensitive_paths)|set(FUNCTIONAL_SOURCE_PATHS)
 for dep in(actual_dependency_record or{}).get('dependencies',[]):
  physical=Path(dep['resolved']);expected=dep.get('expected_sha256');rel=_relative(physical,old_source)
  if expected is None:why.append('Prior current dependency has no authenticated expected hash:'+str(physical));continue
  if rel is not None:
   present=new_tree.get(rel)
   if present!=expected:why.append('Changed or missing genuine project dependency:'+rel)
   if source_changed and rel in sensitive:why.append('Functional/caller-conservative source-path dependency moved (__FILE__/source_location):'+rel)
   checked.append({'prior':str(physical),'current':str(new_source/rel),'expected_sha256':expected,'current_sha256':present,'kind':'project'})
   continue
  alias=generated_aliases.get(str(physical))
  if alias is not None:
   if alias['sha256']!=expected or current_generated_expected.get(alias['key'])!=expected:why.append('Changed/unproved actual generated dependency:'+str(physical))
   checked.append({'prior':str(physical),'current':str(new_build/alias['key']),'expected_sha256':expected,'current_sha256':current_generated_expected.get(alias['key']),'kind':'genuine-owned-generated'})
   continue
  if _relative(physical,old_build)is not None:why.append('Prior build-produced dependency lacks explicit actual generator authority:'+str(physical));continue
  if physical.name in new_headers:why.append('New project header may change actual external include resolution:'+physical.name)
  if not physical.is_file()or hash_file(physical)!=expected:why.append('Current external dependency changed/missing:'+str(physical))
  checked.append({'prior':str(physical),'current':str(physical),'expected_sha256':expected,'kind':'external-current'})
 if actual_dependency_record is not None and not actual_dependency_record.get('dependencies'):why.append('Empty actual dependency witness')
 provider=Path(provider_path)if provider_path is not None else None
 if provider is None or not provider.is_file():why.append('No complete sealed current whole-object provider')
 elif hash_file(provider)!=provider_sha256:why.append('Current whole-object provider bytes changed')
 reasons=sorted(set(why))
 return {'key':key,'route':'compile-complete-TU'if reasons else'reuse-authenticated-current-whole-object','reasons':reasons,'provider':None if reasons else str(provider),'provider_sha256':None if reasons else provider_sha256,'actual_dependency_checks':checked,'source_path_policy':'Known functional __FILE__ paths always fresh on root change; broader caller policy allowed. Literal -D never normalized.','eligibility_scope':'Sealed CURRENT compiler/actual-MD whole providers only; caller must authenticate context and outputs before this function.'}
