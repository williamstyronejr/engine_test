"""Validate counter semantics on isolated report copies, after updating hashes."""
import copy, hashlib, json, subprocess, tempfile
from pathlib import Path
root=Path('validation/2026-10-28-stability')
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
cases += [
 ('warmup',lambda d:d['workload'].update(warmup_per_cycle=600)),
 ('checksum',lambda d:d['run'].update(checksum='0')),
 ('presentation_count',lambda d:d['presentation'].update(present_calls=3600)),
]
sensor_cases=[
 ('missing_sensor',lambda d:d['samples'][0]['values'].pop(next(iter(d['samples'][0]['values'])))),
 ('missing_error',lambda d:d['samples'][0]['values'].__setitem__(next(iter(d['samples'][0]['values'])),{'value':None,'error':None})),
 ('non_numeric',lambda d:d['samples'][0]['values'].__setitem__(next(iter(d['samples'][0]['values'])),{'value':'nan','error':None})),
 ('reversed_time',lambda d:d['samples'][1].update(offset_seconds=-1)),
 ('overlapping_reads',lambda d:d['samples'][0].update(read_seconds=1000)),
 ('wrong_label',lambda d:d['samples'][1].update(label='after')),
 ('invalid_cpu_time',lambda d:d.update(collector_cpu_seconds=-1)),
 ('missing_static',lambda d:d['static_after'].clear()),
]
def unavailable(d):
 for sample in d['samples']:
  for path in sample['values']:sample['values'][path]={'value':None,'error':'synthetic unavailable'}
Path('build/stability-controls').mkdir(parents=True,exist_ok=True)
checks=[]
all_cases=[('valid',lambda d:None,'report',True),('uint64_max',high,'report',True)]
all_cases += [(name,mutate,'report',False) for name,mutate in cases]
all_cases += [('unavailable_sensors',unavailable,'sensors',True)]
all_cases += [(name,mutate,'sensors',False) for name,mutate in sensor_cases]
all_cases += [('run_order',lambda d:d['runs'].reverse(),'manifest',False)]
for name,mutate,kind,accept in all_cases:
 with tempfile.TemporaryDirectory(dir='build/stability-controls') as tmp:
  dest=Path(tmp);current=copy.deepcopy(manifest)
  for e in current['references']+current['runs']:
   for k in ['report','log','sensors']:(dest/e[k]).symlink_to((root/e[k]).resolve())
  (dest/'collect.py').symlink_to((root/'collect.py').resolve())
  if kind=='manifest':mutate(current)
  else:
   entry=current['runs'][0];path=dest/entry[kind]
   data=json.loads(path.read_text());mutate(data);path.unlink();path.write_text(json.dumps(data))
   entry[kind+'_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
  (dest/'manifest.json').write_text(json.dumps(current))
  p=subprocess.run(['python3','-B',str(root/'analyze.py'),str(dest)],capture_output=True,text=True)
  assert (p.returncode==0)==accept,(name,p.returncode,p.stdout,p.stderr)
  checks.append(dict(control=name,exit_code=p.returncode,expected_accept=accept))
(root/'analyzer-controls.json').write_text(json.dumps(checks,indent=2)+'\n')
print(sum(c['expected_accept'] for c in checks),'valid controls accepted;',sum(not c['expected_accept'] for c in checks),'corrupt controls rejected')
