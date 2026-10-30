"""Fresh targeted compiler/sanitizer validation and full-frame fingerprint audit.

Run from the repository root, with installed tools, DISPLAY and ALSA null PCM.
Preserves full CTest output immediately after each run; XML may truncate output.
"""
import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

root = Path('validation/2026-10-30-ui-session')
scratch = Path(Path('build/ui-session-refresh/scratch.txt').read_text().strip())
os.environ['PATH'] = str(Path.cwd()/'.tools/bin')+':'+os.environ['PATH']
os.environ['TMPDIR'] = str(scratch/'compiler-tmp')
assert not (root/'commands.json').exists()
assert not subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], text=True).strip()
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
groups = ['ui_session', 'ui', 'diagnostics', 'font_reload', 'font_graphics', 'renderer_memory']
targets = ['ui_session_tests', 'ui_tests', 'diagnostics_tests', 'font_reload_tests',
           'font_graphics_tests', 'renderer_memory_tests']
states = ['gameplay', 'diagnostics', 'shapes', 'settings', 'muted', 'controls', 'capture',
          'canceled', 'rebound', 'settings_return', 'resumed', 'small_viewport',
          'restored_viewport', 'font_retained', 'font_repaired']
commands, checks, sessions = [], [], []


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


run('environment', ['bash', '-c', 'uname -a; c++ --version; clang++ --version; cmake --version; ninja --version; glxinfo -B'])
for config in ['debug', 'release', 'clang', 'native-sanitize']:
    assert not subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], text=True).strip()
    build = scratch/config
    assert not build.exists()
    configure = ['cmake', '-S', '.', '-B', str(build), '-G', 'Ninja',
                 '-DCMAKE_BUILD_TYPE='+('Release' if config == 'release' else 'Debug'),
                 '-DENGINE_PACKAGE_TEST_ROOT='+str(scratch/'packages')]
    if config == 'clang':
        configure += ['-DCMAKE_CXX_COMPILER=clang++']
    if config == 'native-sanitize':
        configure += ['-DENGINE_SANITIZERS=ON']
    run(config+'-configure', configure)
    run(config+'-build', ['cmake', '--build', str(build), '--target', *targets, '-j2'])
    selection = ['-R', '^('+'|'.join(groups)+')$']
    inventory = json.loads(subprocess.check_output(
        ['ctest', '--test-dir', str(build), '--show-only=json-v1', *selection], text=True))
    assert sorted(t['name'] for t in inventory['tests']) == sorted(groups)
    (root/(config+'-inventory.json')).write_text(json.dumps(inventory, indent=2)+'\n')
    env = {'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'} if config == 'native-sanitize' else {}
    xml = root/(config+'-tests.xml')
    run(config+'-tests', ['ctest', '--test-dir', str(build), *selection, '--output-on-failure',
                         '--output-junit', str(xml.resolve())], env)
    output = (build/'Testing/Temporary/LastTest.log').read_text()
    (root/(config+'-full-output.log')).write_text(output)
    suite = ET.parse(xml).getroot()
    assert int(suite.attrib['tests']) == 6
    assert int(suite.attrib['failures']) == int(suite.attrib['skipped']) == 0
    frames = re.findall(r'^\[ui-frame\] cycle=(\d+) state=(\w+) width=(\d+) height=(\d+) hash=(\d+)$', output, re.M)
    assert [(int(f[0]), f[1]) for f in frames] == [(c, s) for c in range(1, 17) for s in states]
    for _, state, width, height, _ in frames:
        assert (width, height) == (('320', '240') if state == 'small_viewport' else ('640', '480'))
    values = dict(field.split('=') for field in re.search(r'^\[ui-session\] (.+)$', output, re.M).group(1).split())
    comparable = {k: v for k, v in values.items() if k != 'elapsed_seconds'}
    assert comparable == dict(cycles='16', ticks='16664', renders='328', captures='240',
                              presentation_baseline_restored='true')
    if sessions:
        assert frames == sessions[0]['frames'], 'Full-frame fingerprints differ; retain all outputs'
    sessions.append(dict(config=config, workload=comparable, frames=frames,
                         elapsed_seconds=float(values['elapsed_seconds'])))
    checks.append(dict(config=config, groups=groups, failures=0, skips=0))
    (root/'checks.json').write_text(json.dumps(checks, indent=2)+'\n')
    (root/'sessions.json').write_text(json.dumps(sessions, indent=2)+'\n')
print('PASS 24 groups; all 240 frame fingerprints match across four builds', flush=True)
