import json, os, pathlib, subprocess, sys
source, build, evidence, packages = sys.argv[1:]
def command(args):
    print('COMMAND', json.dumps(args), flush=True)
    subprocess.run(args, check=True)
interfaces = sorted(p.name for p in pathlib.Path('/sys/class/net').iterdir())
routes = pathlib.Path('/proc/net/route').read_text()
ipv6 = pathlib.Path('/proc/net/ipv6_route').read_text()
record = dict(namespace=os.readlink('/proc/self/ns/net'), interfaces=interfaces,
              routes=routes, ipv6_routes=ipv6, display=os.environ.get('DISPLAY'))
pathlib.Path(evidence, 'network.json').write_text(json.dumps(record, indent=2)+'\n')
assert interfaces == ['lo'], record
assert len(routes.strip().splitlines()) == 1, record
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
