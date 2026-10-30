import json, os, pathlib, socket, subprocess, sys
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
pathlib.Path(evidence, 'network.json').write_text(json.dumps(record, indent=2)+'\n')
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
