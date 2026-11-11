#!/usr/bin/env python3
"""Run bounded independent GPU controls; write evidence outside tracked sources.

Requires already-built Release and ASan/UBSan consumers. No downloads, builds,
system installation or native application/document mutation occurs here.
"""
import argparse
import array
import hashlib
import json
import os
import pathlib
import subprocess
import sys

experiment = pathlib.Path(__file__).resolve().parent
repository = experiment.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--release-build', type=pathlib.Path, required=True)
parser.add_argument('--sanitize-build', type=pathlib.Path, required=True)
parser.add_argument('--output-dir', type=pathlib.Path, required=True)
parser.add_argument('--profile', choices=['software', 'arc', 'all'], default='all')
args = parser.parse_args()
output = args.output_dir.resolve()
if output == experiment or experiment in output.parents:
    parser.error('Evidence must be outside tracked experiment source directory')
output.mkdir(parents=True, exist_ok=True)
profiles = ['software', 'arc'] if args.profile == 'all' else [args.profile]
receipt = {
    'scope': 'Isolated altered ArmorPaint blur-family extraction; no production backend integration',
    'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repository, text=True).strip(),
    'upstream': {'repository': 'https://github.com/armory3d/armorpaint',
                 'commit': '629f8e77b3f19d533c60a1baa4fe9d4db9ee1163',
                 'license': 'zlib/libpng',
                 'source_sha256': hashlib.sha256((experiment/'vendor/make_blur.c').read_bytes()).hexdigest()},
    'asan_options': 'detect_leaks=1:halt_on_error=1',
    'ubsan_options': 'halt_on_error=1:print_stacktrace=1',
    'lsan_options': None,
    'suppressions': None,
    'timeout_seconds_per_run': 30,
    'maximum_dimension': 512,
    'default_caller_texture_budget_bytes': 4194304,
    'pixel_readback': 'Independent validation consumer only; none in adapter',
    'input_contract': 'Finite linear RGB, [0,1] FLOAT coverage and independent mask supplied by caller',
    'profiles': {}, 'hashes': {}, 'status': 'running',
}
files = ['CMakeLists.txt', 'NOTICE.txt', 'README.md', 'generate_shader.py',
         'blur_adapter.hpp', 'blur_adapter.cpp', 'proof.cpp', 'validate.py',
         'vendor/make_blur.c', 'vendor/LICENSE.armorpaint']
for name in files:
    receipt['hashes']['experiments/armorpaint-blur/'+name] = hashlib.sha256((experiment/name).read_bytes()).hexdigest()
for name in ['graphics.cpp', 'graphics.h', 'LICENSE.native-graphics']:
    receipt['hashes']['third_party/openbrush/'+name] = hashlib.sha256((repository/'third_party/openbrush'/name).read_bytes()).hexdigest()

def seal():
    (output/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')

try:
    for sanitizer, directory in [(False, args.release_build.resolve()), (True, args.sanitize_build.resolve())]:
        for profile in profiles:
            name = profile + ('-sanitize' if sanitizer else '')
            env = os.environ.copy()
            for key in ['LIBGL_ALWAYS_SOFTWARE', 'DRI_PRIME', 'ASAN_OPTIONS', 'LSAN_OPTIONS', 'UBSAN_OPTIONS']:
                env.pop(key, None)
            if profile == 'software':
                env['LIBGL_ALWAYS_SOFTWARE'] = '1'
            else:
                env['DRI_PRIME'] = 'pci-0000_03_00_0'
            if sanitizer:
                env['ASAN_OPTIONS'] = receipt['asan_options']
                env['UBSAN_OPTIONS'] = receipt['ubsan_options']
            command = [str(directory/'blur-proof'), profile, str(output/name)]
            process = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, timeout=30)
            (output/(name+'.log')).write_text(process.stdout)
            result = {'exit_status': process.returncode, 'log': process.stdout}
            receipt['profiles'][name] = result
            if process.returncode != 0:
                raise RuntimeError(name+' failed; original output preserved')
            result.update(json.loads((output/(name+'.json')).read_text()))
            result['raw_sha256'] = hashlib.sha256((output/(name+'.f32')).read_bytes()).hexdigest()
            result['binary_sha256'] = hashlib.sha256((directory/'blur-proof').read_bytes()).hexdigest()
            result['static_library_sha256'] = hashlib.sha256((directory/'libarmorpaint-blur.a').read_bytes()).hexdigest()
            result['generated_fragment_sha256'] = hashlib.sha256((directory/'generated/blur.frag').read_bytes()).hexdigest()
            if 'ERROR:' in process.stdout or 'FAIL' in process.stdout:
                raise RuntimeError(name+' diagnostic was not clean')
            print('PASS', name, result['assertions'])
    for profile in profiles:
        if (output/(profile+'.f32')).read_bytes() != (output/(profile+'-sanitize.f32')).read_bytes():
            raise RuntimeError('Release/SAN output mismatch on '+profile)
    receipt['release_sanitize_outputs_identical_per_device'] = True
    if profiles == ['software', 'arc']:
        outputs = []
        for profile in profiles:
            values = array.array('f')
            values.frombytes((output/(profile+'.f32')).read_bytes())
            outputs.append(values)
        if len(outputs[0]) != len(outputs[1]) or len(outputs[0]) != 15708:
            raise RuntimeError('Cross-device component count changed')
        maximum = 0.0
        for a, b in zip(*outputs):
            delta = abs(a-b)
            if delta > max(4e-12, max(abs(a), abs(b))*6e-6):
                raise RuntimeError('Cross-device float tolerance exceeded')
            maximum = max(maximum, delta)
        receipt['cross_device'] = {'components': len(outputs[0]), 'maximum_absolute_difference': maximum}
    receipt['status'] = 'pass'
    seal()
except Exception as error:
    receipt['status'] = 'failed'
    receipt['failure'] = str(error)
    seal()
    raise
