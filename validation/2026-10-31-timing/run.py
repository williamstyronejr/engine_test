"""Sequential current-baseline refresh; retain measured failures, never retry."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import xml.etree.ElementTree as ET

root = Path('validation/2026-10-31-timing')
scratch = Path(Path('build/timing-baseline-refresh/scratch.txt').read_text().strip())
os.environ['PATH'] = str(Path.cwd()/'.tools/bin')+':'+os.environ['PATH']
os.environ['TMPDIR'] = str(scratch/'compiler-tmp')
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
assert not (root/'commands.json').exists()
assert not subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], text=True).strip()
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
commands = []

def run(name, args, accepted=(0,)):
    stamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    start = time.monotonic()
    print('START', name, flush=True)
    with (root/(name+'.log')).open('x') as log:
        p = subprocess.run(args, stdout=log, stderr=subprocess.STDOUT)
    commands.append(dict(name=name, command=args, source=source, started_utc=stamp,
                         cwd=str(Path.cwd()), seconds=time.monotonic()-start, exit_code=p.returncode,
                         environment={k: os.environ.get(k) for k in
                                      ['PATH','TMPDIR','DISPLAY','PYTHONDONTWRITEBYTECODE']}))
    (root/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
    print('DONE', name, p.returncode, flush=True)
    if p.returncode not in accepted:
        print((root/(name+'.log')).read_text()[-5000:])
        raise SystemExit(p.returncode)

run('environment', ['bash', '-c', 'uname -a; c++ --version; cmake --version; ninja --version; glxinfo -B'])
run('configure', ['cmake','--preset','release','-DENGINE_PACKAGE_TEST_ROOT='+str(scratch/'packages')])
run('build', ['cmake','--build','--preset','release','--target','feature_lab','stress_bench',
              'stress_tests','replay_profile_tests','spike_profile_tests','stress_graphics_tests','-j2'])
run('tests', ['ctest','--preset','release','-R',
             '^(stress_cpu|stress_report|replay_profile|spike_profile|stress_graphics|stress_frame_samples)$',
             '--output-on-failure','--output-junit',str((root/'checks.xml').resolve())])
(root/'tests-full-output.log').write_bytes(Path('build/release/Testing/Temporary/LastTest.log').read_bytes())
suite=ET.parse(root/'checks.xml').getroot()
assert int(suite.attrib['tests'])==6 and int(suite.attrib['failures'])==int(suite.attrib['skipped'])==0
paths=['benchmarks/TIMING_BASELINE_REFRESH.md','benchmarks/WORKLOAD_ACCEPTANCE.md',
       'benchmarks/STRESS_RUN_STABILITY.md','validation/2026-10-26-workload-acceptance/collect.py',
       'validation/2026-10-26-workload-acceptance/policy.json',
       'validation/2026-10-26-workload-acceptance/evaluate.py',
       *map(str,(root/'stability').glob('*.py'))]
provenance=dict(source=source, hashes={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in paths})
assert not subprocess.check_output(['git','diff','--name-only','485de58','HEAD','--',
                                   'engine','examples','tests','CMakeLists.txt'],text=True).strip()
for name in ['collect.py','analyze.py','telemetry.py']:
    assert (root/'stability'/name).read_bytes()==(Path('validation/2026-10-28-stability')/name).read_bytes()
(root/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
run('host-before', ['bash','-c','uptime; cat /proc/loadavg; ps -eo pid,comm,pcpu,pmem --sort=-pcpu | head -16'])
run('acceptance-collection', ['python3','validation/2026-10-26-workload-acceptance/collect.py',str(root/'acceptance')])
run('acceptance-evaluation', ['python3','validation/2026-10-26-workload-acceptance/evaluate.py',str(root/'acceptance')], (0,1))
run('stability-collection', ['python3',str(root/'stability/collect.py'),'build/release/feature_lab',str(root/'stability')])
run('host-after', ['bash','-c','uptime; cat /proc/loadavg; ps -eo pid,comm,pcpu,pmem --sort=-pcpu | head -16'])
run('stability-analysis', ['python3',str(root/'stability/analyze.py'),str(root/'stability')])
run('analyzer-controls', ['python3',str(root/'stability/analyzer_controls.py')])
print('Collection and evidence validation complete; consult timing/stability decisions separately.',flush=True)
