"""Run from the repository root, with installed tools and a working DISPLAY.

Uses existing preset build directories; this is not a clean/offline build claim.
Evidence is append-only: use a new directory for another attempt.
"""
import datetime
import json
import os
from pathlib import Path
import subprocess
import time
import xml.etree.ElementTree as ET

root = Path('validation/2026-10-29-readiness')
scratch = Path('build/readiness-refresh')
assert not (root / 'commands.json').exists()
os.environ['PATH'] = str(Path.cwd() / '.tools/bin') + ':' + os.environ['PATH']
package_root = Path((scratch / 'package-root.txt').read_text().strip())
os.environ['TMPDIR'] = str(package_root.parent / 'compiler-tmp')
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
commands, checks = [], []


def run(name, args, overrides=None):
    environment = os.environ | (overrides or {})
    started = time.monotonic()
    stamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    print('START', name, flush=True)
    with (root / (name + '.log')).open('x') as log:
        result = subprocess.run(args, env=environment, stdout=log, stderr=subprocess.STDOUT)
    commands.append(dict(name=name, command=args, source=source, started_utc=stamp,
                         working_directory=str(Path.cwd()),
                         environment={'PATH': environment['PATH'], 'TMPDIR': environment['TMPDIR'],
                                      'DISPLAY': environment.get('DISPLAY'), **(overrides or {})},
                         seconds=time.monotonic()-started, exit_code=result.returncode))
    (root / 'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
    print('DONE', name, result.returncode, flush=True)
    if result.returncode:
        print((root / (name + '.log')).read_text()[-6000:])
        raise SystemExit(result.returncode)


def test(name, build, selection, overrides):
    inventory = json.loads(subprocess.check_output(
        ['ctest', '--test-dir', str(build), '--show-only=json-v1', *selection], text=True))
    (root / (name+'-inventory.json')).write_text(json.dumps(inventory, indent=2)+'\n')
    xml = root / (name+'-tests.xml')
    run(name+'-tests', ['ctest', '--test-dir', str(build), *selection, '--output-on-failure',
                        '--output-junit', str(xml.resolve())], overrides)
    suite = ET.parse(xml).getroot()
    assert int(suite.attrib['tests']) == len(inventory['tests']) > 0
    assert int(suite.attrib['failures']) == int(suite.attrib['skipped']) == 0
    checks.append(dict(name=name, groups=[t['name'] for t in inventory['tests']],
                       failures=0, skips=0))
    (root / 'checks.json').write_text(json.dumps(checks, indent=2)+'\n')


for config in ['debug', 'release', 'clang', 'sanitize', 'sanitize-platform']:
    assert not subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], text=True).strip()
    build = Path('build') / config
    configure = ['cmake', '--preset', config] if config != 'sanitize-platform' else [
        'cmake', '-S', '.', '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug',
        '-DENGINE_SANITIZERS=ON']
    run(config+'-configure', configure+['-DENGINE_PACKAGE_TEST_ROOT='+str(package_root)])
    build_args = ['cmake', '--build', str(build), '-j2']
    selection, overrides = ['-LE', 'packaging'], {}
    if config.startswith('sanitize'):
        overrides = {'ASAN_OPTIONS': 'detect_leaks='+('1' if config == 'sanitize' else '0')+':halt_on_error=1',
                     'UBSAN_OPTIONS': 'halt_on_error=1'}
    if config == 'sanitize-platform':
        groups = ['renderer_memory', 'render_targets', 'font_graphics', 'font_reload',
                  'texture_reload', 'shaders', 'watched_reload', 'scene_watch', 'tilemap_watch',
                  'animation_watch', 'stress_graphics', 'stress_frame_samples', 'ui_allocations',
                  'diagnostics_allocations', 'audio_transport', 'audio_reload']
        selection = ['-R', '^('+'|'.join(groups)+')$']
        build_args += ['--target', 'renderer_memory_tests', 'render_target_tests', 'font_graphics_tests',
                       'font_reload_tests', 'texture_reload_tests', 'shader_tests', 'watched_reload_tests',
                       'scene_watch_tests', 'tilemap_watch_tests', 'animation_watch_tests',
                       'stress_graphics_tests', 'feature_lab', 'ui_allocations',
                       'diagnostics_allocations', 'audio_tests', 'audio_reload_tests']
    run(config+'-build', build_args)
    test(config, build, selection, overrides)
    if config == 'sanitize-platform':
        test('audio-leaks', build, ['-R', '^(audio_transport|audio_reload)$'],
             overrides | {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1'})

tsan = Path('build/readiness-tsan')
assert not tsan.exists(), 'TSan build must be new for this record'
run('tsan-configure', ['cmake', '-S', '.', '-B', str(tsan), '-G', 'Ninja',
                       '-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_CXX_COMPILER=clang++',
                       '-DENGINE_SANITIZERS=OFF', '-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer',
                       '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread',
                       '-DENGINE_PACKAGE_TEST_ROOT='+str(package_root)])
run('tsan-build', ['cmake', '--build', str(tsan), '--target', 'audio_tests', 'audio_reload_tests', '-j2'])
test('tsan-audio', tsan, ['-R', '^(audio_transport|audio_reload)$'], {'TSAN_OPTIONS': 'halt_on_error=1'})
print('PASS', sum(len(c['groups']) for c in checks), 'groups', flush=True)
