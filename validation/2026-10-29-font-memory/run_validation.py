"""Sequential correctness matrix. Retain commands, reports, images hashes and failures."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

root = Path('validation/2026-10-29-font-memory')
scratch = Path('build/font-memory-validation')
assert not (root / 'commands.json').exists()
os.environ['PATH'] = str(Path.cwd() / '.tools/bin') + ':' + os.environ['PATH']
temp = Path(tempfile.mkdtemp(prefix='engine-font-memory-', dir='/home/zero/.cache'))
os.environ['TMPDIR'] = str(temp / 'compiler-tmp')
Path(os.environ['TMPDIR']).mkdir()
(scratch / 'package-root.txt').write_text(str(temp / 'packages') + '\n')
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
commands = []
images = []
checks = []


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(name, args, overrides=None):
    environment = os.environ | (overrides or {})
    stamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    started = time.monotonic()
    print('START', name, flush=True)
    with (root / (name + '.log')).open('x') as log:
        p = subprocess.run(args, env=environment, stdout=log, stderr=subprocess.STDOUT)
    commands.append(dict(name=name, command=args, source=source, working_directory=str(Path.cwd()),
                         environment={'PATH': environment['PATH'], 'TMPDIR': environment['TMPDIR'],
                                      'DISPLAY': environment.get('DISPLAY'), **(overrides or {})},
                         started_utc=stamp, seconds=time.monotonic()-started, exit_code=p.returncode))
    (root / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    print('DONE', name, p.returncode, flush=True)
    if p.returncode:
        print((root / (name + '.log')).read_text()[-5000:])
        raise SystemExit(p.returncode)


reference_script = scratch / 'reference-frames.cmake'
reference_script.write_bytes(subprocess.check_output(['git', 'show', 'b284d2e:tests/check_stress_frames.cmake']))
reference = scratch / 'reference-feature-lab'
run('reference', ['cmake', '-DGAME_EXE='+str(reference.resolve()),
                 '-DOUTPUT_DIR='+str((scratch/'reference-run').resolve()), '-P', str(reference_script)])
(root / 'reference.json').write_text(json.dumps(dict(source='b284d2e58122eb4f6266de491342b3228c281fcc',
    executable_sha256=digest(reference), test_script_sha256=digest(reference_script)), indent=2)+'\n')
native = ['renderer_memory', 'stress_graphics', 'stress_frame_samples', 'font_graphics', 'font_reload']
cpu = ['fonts', 'stress_cpu', 'stress_report']
targets = ['renderer_memory_tests', 'stress_graphics_tests', 'font_graphics_tests',
           'font_reload_tests', 'font_tests', 'feature_lab', 'stress_tests', 'stress_bench']
configs = [('debug', native+cpu, {}), ('release', native+cpu, {}), ('clang', native+cpu, {}),
           ('sanitize', cpu, {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'}),
           ('sanitize-platform', native, {'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'})]
for config, groups, overrides in configs:
    build = Path('build') / config
    configure = ['cmake', '--preset', config] if config != 'sanitize-platform' else [
        'cmake', '-S', '.', '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug', '-DENGINE_SANITIZERS=ON']
    configure += ['-DENGINE_PACKAGE_TEST_ROOT='+str(temp/'packages')]
    run(config+'-configure', configure)
    run(config+'-build', ['cmake', '--build', str(build), '--target',
                         *(targets if config != 'sanitize' else ['font_tests', 'stress_tests', 'stress_bench']), '-j2'])
    xml = root / (config+'-tests.xml')
    run(config+'-tests', ['ctest', '--test-dir', str(build), '-R', '^('+'|'.join(groups)+')$',
                         '--output-on-failure', '--output-junit', str(xml.resolve())], overrides)
    suite = ET.parse(xml).getroot()
    assert int(suite.attrib['tests']) == len(groups)
    assert int(suite.attrib['failures']) == int(suite.attrib['skipped']) == 0
    checks.append(dict(configuration=config, groups=groups, failures=0, skips=0))
    if config != 'sanitize':
        for mode in ['plain', 'traced', 'vsync', 'split', 'split_vsync', 'prep', 'prep_vsync', 'ctx', 'ctx_vsync']:
            before = scratch / 'reference-run' / (mode+'.ppm')
            after = build / 'stress-frame-tests' / (mode+'.ppm')
            assert digest(before) == digest(after), (config, mode)
            images.append(dict(configuration=config, mode=mode, sha256=digest(after)))
            shutil.copy2(after.with_suffix('.json'), root / (config+'-'+mode+'.json'))
    (root/'checks.json').write_text(json.dumps(checks, indent=2)+'\n')
    (root/'images.json').write_text(json.dumps(images, indent=2)+'\n')
print('PASS', sum(len(c['groups']) for c in checks), 'groups;', len(images), 'reference image matches', flush=True)
