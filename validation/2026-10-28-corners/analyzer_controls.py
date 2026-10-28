"""Validate counter semantics on isolated report copies, after updating hashes."""
import copy, hashlib, json, subprocess, tempfile
from pathlib import Path
root=Path('validation/2026-10-28-corners')
manifest=json.loads((root/'manifest.json').read_text())
def row(d):return d['context_samples']['rows'][0]
def phase(d):return row(d)['phases'][0]
def high(d):
 for r in d['context_samples']['rows']:
  for p in r['phases']:p.update(before=[str(2**64-1)]*2,after=[str(2**64-1)]*2)
cases=[
 ('source_revision',lambda d:d['build'].update(revision='wrong')),
 ('build_flags',lambda d:d['build'].update(flags='-O0; sanitizers=OFF')),
 ('workload',lambda d:d['workload'].update(sprites=1)),
 ('render_totals',lambda d:d['totals'].update(quads=1)),
 ('phase_cpu_nesting',lambda d:d['preparation_samples']['rows'][0]['phases'][0].__setitem__(3,1000)),
 ('frame_partition',lambda d:d['frame_samples']['rows'][0]['cpu_ms'].__setitem__(0,1000)),
 ('tag',lambda d:row(d).update(frame='62')),
 ('source',lambda d:d['context_samples'].update(source='getrusage(RUSAGE_SELF)')),
 ('phase_count',lambda d:row(d)['phases'].pop()),
 ('counter_count',lambda d:phase(d)['before'].pop()),
 ('numeric_not_string',lambda d:phase(d)['before'].__setitem__(0,0)),
 ('negative',lambda d:phase(d)['before'].__setitem__(0,'-1')),
 ('leading_zero',lambda d:phase(d)['before'].__setitem__(0,'00')),
 ('overflow',lambda d:phase(d)['after'].__setitem__(0,str(2**64))),
 ('within_span_regression',lambda d:phase(d).update(before=['2','2'],after=['1','1'])),
 ('between_phase_regression',lambda d:(phase(d).update(after=[str(2**64-1)]*2),row(d)['phases'][1].update(before=['0','0'],after=['0','0']))),
 ('between_row_regression',lambda d:(row(d)['phases'][-1].update(after=[str(2**64-1)]*2),d['context_samples']['rows'][1]['phases'][0].update(before=['0','0'],after=['0','0']))),
 ('memory',lambda d:d['memory'].update(context_sample_buffer_bytes=0)),
 ('row_count',lambda d:d['context_samples']['rows'].pop()),
 ('counter_order',lambda d:d['context_samples'].update(counter_order=['involuntary','voluntary'])),
]
Path('build/corner-controls').mkdir(parents=True,exist_ok=True)
checks=[]
for name,mutate in [('valid',lambda d:None),('uint64_max',high),*cases]:
 with tempfile.TemporaryDirectory(dir='build/corner-controls') as tmp:
  dest=Path(tmp);current=copy.deepcopy(manifest)
  for e in current['runs']:
   for k in ['report','log']:(dest/e[k]).write_bytes((root/e[k]).read_bytes())
  (dest/'collect.py').write_bytes((root/'collect.py').read_bytes())
  path=dest/'candidate-1.json';data=json.loads(path.read_text());mutate(data)
  path.write_text(json.dumps(data))
  next(e for e in current['runs'] if e['report']==path.name)['report_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
  (dest/'manifest.json').write_text(json.dumps(current))
  p=subprocess.run(['python3',str(root/'analyze.py'),str(dest)],capture_output=True,text=True)
  accept=name in ('valid','uint64_max')
  assert (p.returncode==0)==accept,(name,p.returncode,p.stdout,p.stderr)
  checks.append(dict(control=name,exit_code=p.returncode,expected_accept=accept))
(root/'analyzer-controls.json').write_text(json.dumps(checks,indent=2)+'\n')
print('Valid/full-width counters accepted; twenty corrupted records rejected')
