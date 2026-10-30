"""Run the committed session fixture and related regressions in fresh target builds.

From the repository root; requires installed compilers, DISPLAY and ALSA null PCM.
Builds only selected targets, not a fresh full engine/toolchain qualification.
"""
import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

root = Path('validation/2026-10-30-long-session')
scratch = Path(Path('build/long-session-refresh/scratch.txt').read_text().strip())
os.environ['PATH'] = str(Path.cwd()/'.tools/bin')+':'+os.environ['PATH']
os.environ['TMPDIR'] = str(scratch/'compiler-tmp')
resume = sys.argv[1:] == ['--resume-after-output-check']
assert not sys.argv[1:] or resume
if not resume:
    assert not (root/'commands.json').exists()
assert not subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], text=True).strip()
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
groups = ['long_session', 'levels', 'persistence', 'scene_reload', 'texture_reload',
          'audio_reload', 'renderer_memory']
targets = ['long_session_tests', 'level_tests', 'persistence_tests', 'scene_reload_tests',
           'texture_reload_tests', 'audio_reload_tests', 'renderer_memory_tests']
commands, checks, sessions = [], [], []
if resume:
    commands = json.loads((root/'commands.json').read_text())
    assert commands[-1]['name'] == 'debug-tests' and commands[-1]['exit_code'] == 0
    assert not (root/'checks.json').exists()
    assert not subprocess.check_output(
        ['git', 'diff', '--name-only', commands[0]['source'], 'HEAD', '--',
         'tests', 'engine', 'examples', 'CMakeLists.txt'], text=True).strip()


def run(name, args, overrides=None):
    env = os.environ | (overrides or {})
    start = time.monotonic()
    stamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    print('START', name, flush=True)
    with (root/(name+'.log')).open('x') as log:
        p = subprocess.run(args, env=env, stdout=log, stderr=subprocess.STDOUT)
    commands.append(dict(name=name, command=args, source=source, started_utc=stamp,
                         cwd=str(Path.cwd()), seconds=time.monotonic()-start, exit_code=p.returncode,
                         environment={k: env.get(k) for k in
                                      ['PATH', 'TMPDIR', 'DISPLAY', 'ASAN_OPTIONS', 'UBSAN_OPTIONS']}))
    (root/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
    print('DONE', name, p.returncode, flush=True)
    if p.returncode:
        print((root/(name+'.log')).read_text()[-7000:])
        raise SystemExit(p.returncode)


if not resume:
    run('environment', ['bash', '-c', 'uname -a; c++ --version; clang++ --version; cmake --version; ninja --version; glxinfo -B'])
for config in ['debug', 'release', 'clang', 'native-sanitize']:
    build = scratch/config
    reuse_completed = resume and config == 'debug'
    assert reuse_completed or not build.exists()
    configure = ['cmake', '-S', '.', '-B', str(build), '-G', 'Ninja',
                 '-DCMAKE_BUILD_TYPE='+('Release' if config == 'release' else 'Debug'),
                 '-DENGINE_PACKAGE_TEST_ROOT='+str(scratch/'packages')]
    if config == 'clang':
        configure += ['-DCMAKE_CXX_COMPILER=clang++']
    if config == 'native-sanitize':
        configure += ['-DENGINE_SANITIZERS=ON']
    if not reuse_completed:
        run(config+'-configure', configure)
        run(config+'-build', ['cmake', '--build', str(build), '--target', *targets, '-j2'])
    selection = ['-R', '^('+'|'.join(groups)+')$']
    inventory = json.loads(subprocess.check_output(
        ['ctest', '--test-dir', str(build), '--show-only=json-v1', *selection], text=True))
    assert sorted(t['name'] for t in inventory['tests']) == sorted(groups)
    (root/(config+'-inventory.json')).write_text(json.dumps(inventory, indent=2)+'\n')
    env = {'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'} if config == 'native-sanitize' else {}
    xml = root/(config+'-tests.xml')
    if not reuse_completed:
        run(config+'-tests', ['ctest', '--test-dir', str(build), *selection, '--output-on-failure',
                             '--output-junit', str(xml.resolve())], env)
    suite = ET.parse(xml).getroot()
    assert int(suite.attrib['tests']) == 7
    assert int(suite.attrib['failures']) == int(suite.attrib['skipped']) == 0
    # CTest truncates successful JUnit output to 1 KiB by default. Preserve and
    # inspect the full LastTest log; XML still supplies pass/fail/skip status.
    output = (build/'Testing/Temporary/LastTest.log').read_text()
    (root/(config+'-full-output.log')).write_text(output)
    cycles = re.findall(r'^\[cycle\] completed=(\d+) ticks=(\d+) scene_bytes=(\d+) sound_bytes=(\d+)$', output, re.M)
    assert [int(c[0]) for c in cycles] == list(range(1, 65))
    summary = re.search(r'^\[session\] (.+)$', output, re.M).group(1)
    values = dict(field.split('=') for field in summary.split())
    assert values['cycles'] == '64' and values['audio_outputs'] == '8'
    assert values['resources_restored'] == 'true'
    assert sum(int(c[1]) for c in cycles) == int(values['ticks'])
    comparable = {k: v for k, v in values.items() if k != 'elapsed_seconds'}
    if sessions:
        assert comparable == sessions[0]['workload']
        assert cycles == sessions[0]['cycles']
    sessions.append(dict(config=config, workload=comparable, cycles=cycles,
                         elapsed_seconds=float(values['elapsed_seconds'])))
    checks.append(dict(config=config, groups=groups, failures=0, skips=0))
    (root/'checks.json').write_text(json.dumps(checks, indent=2)+'\n')
    (root/'sessions.json').write_text(json.dumps(sessions, indent=2)+'\n')
print('PASS 28 groups; all four session workloads match', flush=True)
