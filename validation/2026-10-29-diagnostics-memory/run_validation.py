"""Sequential overlay correctness, allocation and capture matrix; run from repo root.

Preserve pre-change Release ui_allocations and feature_lab in the scratch directory
as reference-ui and reference-feature-lab before rebuilding. Never overwrites evidence.
"""
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import xml.etree.ElementTree as ET

root = Path('validation/2026-10-29-diagnostics-memory')
scratch = Path('build/diagnostics-memory-validation')
assert not (root / 'commands.json').exists()
os.environ['PATH'] = str(Path.cwd() / '.tools/bin') + ':' + os.environ['PATH']
package_root = Path((scratch / 'package-root.txt').read_text().strip())
os.environ['TMPDIR'] = str(package_root.parent / 'compiler-tmp')
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
commands, checks, images = [], [], []


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(name, args, overrides=None):
    environment = os.environ | (overrides or {})
    started = time.monotonic()
    print('START', name, flush=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    with (root / (name + '.log')).open('x') as log:
        result = subprocess.run(args, env=environment, stdout=log, stderr=subprocess.STDOUT)
    commands.append(dict(name=name, command=args, source=source, started_utc=stamp,
                         environment={'TMPDIR': environment['TMPDIR'],
                                      'DISPLAY': environment.get('DISPLAY'), **(overrides or {})},
                         seconds=time.monotonic()-started, exit_code=result.returncode))
    (root / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    print('DONE', name, result.returncode, flush=True)
    if result.returncode:
        print((root / (name + '.log')).read_text()[-5000:])
        raise SystemExit(result.returncode)


def report(path, expected_count):
    data = json.loads(path.read_text())
    assert data['complete'] and data['diagnostics_layout'] == 'memory_v1'
    assert len(data['scenarios']) == expected_count and data['measured_frames'] == 270
    for scenario in data['scenarios']:
        for scope in scenario['scopes'].values():
            assert scope['calls'] == scope['requested_bytes'] == scope['nonzero_samples'] == 0
            assert scope['samples'] == 270
    return data


run('reference-ui', [str(scratch / 'reference-ui'), str(root / 'reference-ui.json'),
                     str(scratch / 'reference-ui-images')])
reference_script = scratch / 'reference-frames.cmake'
reference_script.write_bytes(subprocess.check_output(['git', 'show', '1b808aa:tests/check_stress_frames.cmake']))
run('reference-frames', ['cmake', '-DGAME_EXE='+str((scratch / 'reference-feature-lab').resolve()),
                         '-DOUTPUT_DIR='+str((scratch / 'reference-frames').resolve()),
                         '-P', str(reference_script)])
(root / 'reference.json').write_text(json.dumps(dict(
    ui_revision=json.loads((root / 'reference-ui.json').read_text())['revision'],
    ui_executable_sha256=digest(scratch / 'reference-ui'),
    feature_lab_revision='1b808aaade22883ea2d189d5c75b0806f7790bc6',
    feature_lab_executable_sha256=digest(scratch / 'reference-feature-lab')), indent=2)+'\n')
cpu = ['hud_text', 'diagnostics']
native = ['render_targets', 'renderer_memory', 'ui_allocations', 'diagnostics_allocations',
          'stress_frame_samples']
targets = ['hud_text_tests', 'diagnostics_tests', 'render_target_tests', 'renderer_memory_tests',
           'ui_allocations', 'diagnostics_allocations', 'feature_lab']
configs = [('release', cpu+native, {}), ('debug', cpu+native, {}), ('clang', cpu+native, {}),
           ('sanitize', cpu, {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'}),
           ('sanitize-platform', native, {'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'})]
for config, groups, overrides in configs:
    build = Path('build') / config
    configure = ['cmake', '--preset', config] if config != 'sanitize-platform' else [
        'cmake', '-S', '.', '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug', '-DENGINE_SANITIZERS=ON']
    run(config+'-configure', configure+['-DENGINE_PACKAGE_TEST_ROOT='+str(package_root)])
    run(config+'-build', ['cmake', '--build', str(build), '--target',
                         *(targets if config != 'sanitize' else targets[:2]), '-j2'])
    xml = root / (config+'-tests.xml')
    run(config+'-tests', ['ctest', '--test-dir', str(build), '-R', '^('+'|'.join(groups)+')$',
                         '--output-on-failure', '--output-junit', str(xml.resolve())], overrides)
    suite = ET.parse(xml).getroot()
    assert int(suite.attrib['tests']) == len(groups)
    assert int(suite.attrib['failures']) == int(suite.attrib['skipped']) == 0
    checks.append(dict(configuration=config, groups=groups, failures=0, skips=0))
    if config != 'sanitize':
        for probe, count in [('ui', 18), ('diagnostics', 12)]:
            data = report(build / (probe+'-allocations.json'), count)
            shutil.copy2(build / (probe+'-allocations.json'), root / (config+'-'+probe+'.json'))
        for path in sorted((scratch / 'reference-frames').glob('*.ppm')):
            after = build / 'stress-frame-tests' / path.name
            assert digest(path) == digest(after), (config, path.name)
            images.append(dict(configuration=config, kind='unchanged_stress', name=path.name, sha256=digest(after)))
        capture_dir = scratch / (config+'-diagnostics-images')
        capture_report = root / (config+'-diagnostics-capture.json')
        run(config+'-capture', [str(build / 'diagnostics_allocations'), str(capture_report),
                                str(capture_dir)], overrides)
        report(capture_report, 12)
        for path in sorted(capture_dir.glob('*.ppm')):
            baseline = scratch / 'release-diagnostics-images' / path.name
            assert digest(baseline) == digest(path), (config, path.name)
            images.append(dict(configuration=config, kind='new_overlay', name=path.name, sha256=digest(path)))
    (root / 'checks.json').write_text(json.dumps(checks, indent=2)+'\n')
    (root / 'images.json').write_text(json.dumps(images, indent=2)+'\n')

run('release-ui-capture', ['build/release/ui_allocations', str(root / 'release-ui-capture.json'),
                          str(scratch / 'release-ui-images')])
after_ui = report(root / 'release-ui-capture.json', 18)
before_ui = json.loads((root / 'reference-ui.json').read_text())
for before, after in zip(before_ui['scenarios'], after_ui['scenarios'], strict=True):
    for field in ['level', 'name', 'state', 'hash', 'ticks', 'transitions']:
        assert before[field] == after[field], (before['name'], field)
    if not before['state']['diagnostics']:
        name = before['level'].upper()+'-'+before['name']+'.ppm'
        assert before['render'] == after['render']
        assert digest(scratch / 'reference-ui-images' / name) == digest(scratch / 'release-ui-images' / name)
        images.append(dict(configuration='release', kind='unchanged_ui', name=name,
                           sha256=digest(scratch / 'release-ui-images' / name)))
for width, height in [(960, 540), (640, 360), (320, 240), (1050, 1360)]:
    label = str(width)+'x'+str(height)
    capture = root / ('viewport-'+label+'.json')
    run('viewport-'+label, ['build/release/diagnostics_allocations', str(capture),
                            str(scratch / ('viewport-'+label)), str(width), str(height)])
    data = report(capture, 12)
    assert data['width'] == width and data['height'] == height
    for path in sorted((scratch / ('viewport-'+label)).glob('*.ppm')):
        images.append(dict(configuration='release', kind='viewport', viewport=label,
                           name=path.name, sha256=digest(path)))
(root / 'images.json').write_text(json.dumps(images, indent=2)+'\n')
print('PASS', sum(len(c['groups']) for c in checks), 'groups;', len(images), 'image records', flush=True)
