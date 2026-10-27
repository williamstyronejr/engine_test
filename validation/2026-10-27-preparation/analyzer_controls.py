"""Semantic controls run on isolated copies with updated hashes, never original reports."""
import copy, hashlib, json, subprocess, tempfile
from pathlib import Path
root=Path('validation/2026-10-27-preparation')
manifest=json.loads((root/'manifest.json').read_text())
def phase(d):return d['preparation_samples']['rows'][0]['phases'][1]
cases=[
 ('frame_tag',lambda d:d['preparation_samples']['rows'][0].update(frame='62')),
 ('negative',lambda d:phase(d).__setitem__(0,-1)),
 ('nonfinite',lambda d:phase(d).__setitem__(1,float('inf'))),
 ('phase_count',lambda d:d['preparation_samples']['rows'][0]['phases'].pop()),
 ('metric_count',lambda d:phase(d).pop()),
 ('gl_cpu_nesting',lambda d:phase(d).__setitem__(3,phase(d)[1]+1)),
 ('gl_wall_nesting',lambda d:phase(d).__setitem__(2,phase(d)[0]+1)),
 ('outer_wall',lambda d:phase(d).__setitem__(0,1000)),
 ('gl_reconstruction',lambda d:phase(d).__setitem__(2,phase(d)[2]+0.00001)),
 ('payload_bytes',lambda d:d['memory'].update(preparation_sample_buffer_bytes=0)),
 ('clock_identity',lambda d:d['preparation_samples'].update(clock='CLOCK_REALTIME')),
 ('row_count',lambda d:d['preparation_samples']['rows'].pop()),
]
Path('build/preparation-controls').mkdir(parents=True,exist_ok=True)
checks=[]
for name,mutate in [('valid',lambda d:None),('signed_difference',lambda d:phase(d).__setitem__(1,phase(d)[0]+0.001)),*cases]:
 with tempfile.TemporaryDirectory(dir='build/preparation-controls') as tmp:
  dest=Path(tmp);current=copy.deepcopy(manifest)
  for e in current['runs']:
   for k in ['report','log']:(dest/e[k]).write_bytes((root/e[k]).read_bytes())
  (dest/'collect.py').write_bytes((root/'collect.py').read_bytes())
  path=dest/'profile-1.json';data=json.loads(path.read_text());mutate(data)
  path.write_text(json.dumps(data))
  next(e for e in current['runs'] if e['report']==path.name)['report_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
  (dest/'manifest.json').write_text(json.dumps(current))
  p=subprocess.run(['python3',str(root/'analyze.py'),str(dest)],capture_output=True,text=True)
  accept=name in ('valid','signed_difference')
  assert (p.returncode==0)==accept,(name,p.returncode,p.stdout,p.stderr)
  checks.append(dict(control=name,exit_code=p.returncode,expected_accept=accept))
(root/'analyzer-controls.json').write_text(json.dumps(checks,indent=2)+'\n')
print('Valid/signed-difference inputs accepted; twelve corrupted records rejected')
