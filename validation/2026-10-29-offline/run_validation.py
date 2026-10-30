"""Fresh native ASan/UBSan and network-isolated Release validation.

Run from the repository root. Installed tools and a working DISPLAY are required.
Uses a fixed committed source archive, new disk-backed scratch and append-only logs.
"""
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import time
import xml.etree.ElementTree as ET

repo = Path.cwd()
evidence = repo / 'validation/2026-10-29-offline'
resume = sys.argv[1:] == ['--resume-offline']
assert not sys.argv[1:] or resume
if not resume:
    assert not (evidence / 'commands.json').exists()
revision = '4f603120812a8b3eca2d2ad7e014329e2e536b19'
if resume:
    scratch = Path(json.loads((evidence/'source.json').read_text())['scratch'])
    commands = json.loads((evidence/'commands.json').read_text())
    checks = json.loads((evidence/'checks.json').read_text())
    assert commands[-1]['name'] == 'offline-build' and commands[-1]['exit_code'] != 0
    assert not (scratch/'offline-release').exists(), 'Only resume a pre-configure failure'
else:
    scratch = Path(tempfile.mkdtemp(prefix='engine-offline-75-', dir=Path.home()/'.cache'))
    (scratch / 'compiler-tmp').mkdir()
    commands, checks = [], []
os.environ['TMPDIR'] = str(scratch / 'compiler-tmp')
os.environ['PATH'] = str(repo / '.tools/bin') + ':' + os.environ['PATH']
(repo / 'build/offline-refresh/scratch.txt').write_text(str(scratch)+'\n')


def run(name, args, env=None):
    started = time.monotonic()
    environment = os.environ | (env or {})
    print('START', name, flush=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    with (evidence / (name+'.log')).open('x') as log:
        result = subprocess.run([str(a) for a in args], env=environment,
                                stdout=log, stderr=subprocess.STDOUT)
    commands.append(dict(name=name, command=[str(a) for a in args], source=revision,
                         started_utc=stamp, seconds=time.monotonic()-started,
                         cwd=str(repo), exit_code=result.returncode,
                         environment={k: environment.get(k) for k in
                                      ['PATH', 'TMPDIR', 'DISPLAY', 'ASAN_OPTIONS', 'UBSAN_OPTIONS']}))
    (evidence/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
    print('DONE', name, result.returncode, flush=True)
    if result.returncode:
        print((evidence/(name+'.log')).read_text()[-6000:])
        raise SystemExit(result.returncode)


def test(name, build, selection, expected, env=None, prefix=None):
    inventory = json.loads(subprocess.check_output(
        ['ctest', '--test-dir', str(build), '--show-only=json-v1', *selection], text=True))
    (evidence/(name+'-inventory.json')).write_text(json.dumps(inventory, indent=2)+'\n')
    assert len(inventory['tests']) == expected
    xml = evidence/(name+'-tests.xml')
    run(name+'-tests', (prefix or [])+['ctest', '--test-dir', build, *selection,
         '--output-on-failure', '--output-junit', xml], env)
    suite = ET.parse(xml).getroot()
    assert int(suite.attrib['tests']) == expected
    assert int(suite.attrib['failures']) == int(suite.attrib['skipped']) == 0
    checks.append(dict(name=name, groups=[t['name'] for t in inventory['tests']], failures=0, skips=0))
    (evidence/'checks.json').write_text(json.dumps(checks, indent=2)+'\n')


def package_identity(name, build):
    archive = Path((build/'packaging/latest.txt').read_text().strip())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    assert archive.with_suffix(archive.suffix+'.sha256').read_text().split()[0] == digest
    record = dict(archive=str(archive), sha256=digest, bytes=archive.stat().st_size, source=revision)
    (evidence/(name+'-package.json')).write_text(json.dumps(record, indent=2)+'\n')
    with tarfile.open(archive) as tar:
        for filename in ['BUILD_INFO.txt', 'RUNTIME_DEPENDENCIES.txt', 'FILES.sha256']:
            member = next(m for m in tar.getmembers() if m.name.endswith('/'+filename))
            data = tar.extractfile(member).read()
            (evidence/(name+'-'+filename+'.log')).write_bytes(data)
            if filename == 'BUILD_INFO.txt':
                assert b'Revision (at configure): unknown\n' in data
    return archive


source = scratch/'source'
if not resume:
    archive = scratch/'source.tar'
    with archive.open('xb') as stream:
        subprocess.run(['git', 'archive', revision], stdout=stream, check=True)
    source = scratch/'source'
    source.mkdir()
    with tarfile.open(archive) as tar:
        tar.extractall(source, filter='data')
    (evidence/'source.json').write_text(json.dumps(dict(revision=revision, archive=str(archive),
        sha256=hashlib.sha256(archive.read_bytes()).hexdigest(), bytes=archive.stat().st_size,
        source=str(source), scratch=str(scratch), embedded_revision='unknown'), indent=2)+'\n')
    run('environment', ['bash', '-c', 'uname -a; cat /etc/os-release; c++ --version; clang++ --version; cmake --version; ninja --version; pkg-config --modversion x11 alsa; glxinfo -B'])
    native = scratch/'native-sanitize'
    run('native-configure', ['cmake', '-S', source, '-B', native, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Debug', '-DENGINE_SANITIZERS=ON',
        '-DENGINE_PACKAGE_TEST_ROOT='+str(scratch/'native-packages')])
    run('native-build', ['cmake', '--build', native, '-j2'])
    san = {'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'}
    test('native-sanitize', native, ['-LE', 'packaging'], 66, san)
    test('native-audio-leaks', native, ['-R', '^(audio_transport|audio_reload)$'], 2,
         san | {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1'})

# The entire configure/build/CPU/package sequence runs in one network namespace.
# Namespace topology is asserted before any build commands, not inferred from success.
offline = scratch/'offline-release'
inside = scratch/'isolated.py'
inside.write_text('''import json, os, pathlib, socket, subprocess, sys
source, build, evidence, packages = sys.argv[1:]
def command(args):
    print('COMMAND', json.dumps(args), flush=True)
    subprocess.run(args, check=True)
interfaces = sorted(name for _, name in socket.if_nameindex())
route4 = json.loads(subprocess.check_output(['ip', '-json', '-4', 'route', 'show', 'table', 'all'], text=True))
route6 = json.loads(subprocess.check_output(['ip', '-json', '-6', 'route', 'show', 'table', 'all'], text=True))
routes = pathlib.Path('/proc/net/route').read_text()
ipv6 = pathlib.Path('/proc/net/ipv6_route').read_text()
record = dict(namespace=os.readlink('/proc/self/ns/net'), parent_namespace=os.environ['AUDIT_PARENT_NETWORK'],
              interfaces=interfaces, netlink_ipv4_routes=route4, netlink_ipv6_routes=route6,
              routes=routes, ipv6_routes=ipv6, display=os.environ.get('DISPLAY'))
pathlib.Path(evidence, 'network.json').write_text(json.dumps(record, indent=2)+'\\n')
assert interfaces == ['lo'], record
assert record['namespace'] != record['parent_namespace'], record
assert not route4, record
assert all(route.get('dev') == 'lo' for route in route6), record
assert all(line.split()[-1] == 'lo' for line in ipv6.splitlines()), record
assert not os.environ.get('DISPLAY')
command(['cmake', '-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
         '-DENGINE_PACKAGE_TEST_ROOT='+packages])
command(['cmake', '--build', build, '-j2'])
inventory = subprocess.check_output(['ctest', '--test-dir', build, '-L', 'cpu', '--show-only=json-v1'], text=True)
pathlib.Path(evidence, 'offline-cpu-inventory.json').write_text(inventory)
assert len(json.loads(inventory)['tests']) == 36
command(['ctest', '--test-dir', build, '-L', 'cpu', '--output-on-failure',
         '--output-junit', str(pathlib.Path(evidence, 'offline-cpu-tests.xml'))])
command(['cmake', '--build', build, '--target', 'release_package'])
''')
(evidence/'isolated.py').write_text(inside.read_text())
run('offline-corrected-build' if resume else 'offline-build', ['unshare', '--user', '--map-root-user', '--net',
    'env', '-u', 'DISPLAY', 'AUDIT_PARENT_NETWORK='+os.readlink('/proc/self/ns/net'), sys.executable, inside, source, offline, evidence, scratch/'offline-packages'])
suite = ET.parse(evidence/'offline-cpu-tests.xml').getroot()
assert int(suite.attrib['tests']) == 36
assert int(suite.attrib['failures']) == int(suite.attrib['skipped']) == 0
inventory = json.loads((evidence/'offline-cpu-inventory.json').read_text())
checks.append(dict(name='offline-cpu', groups=[t['name'] for t in inventory['tests']], failures=0, skips=0))
package_identity('offline-isolated', offline)
test('offline-package', offline, ['-L', 'packaging'], 3)
tested = package_identity('offline-tested', offline)
run('offline-audit', [sys.executable, repo/'validation/2026-10-29-readiness/audit_package.py',
                      tested, evidence/'offline-audit'])
print('PASS', sum(len(c['groups']) for c in checks), 'groups', flush=True)
