"""Exercise semantic rejection after updating report hashes in isolated copies."""
import copy, hashlib, json, subprocess, tempfile
from pathlib import Path
root=Path('validation/2026-10-27-submission')
manifest=json.loads((root/'manifest.json').read_text())
cases=[
 ('frame_tag',lambda d:d['submission_samples']['rows'][0].update(frame='62')),
 ('negative',lambda d:d['submission_samples']['rows'][0]['cpu_ms'].__setitem__(0,-1)),
 ('partition',lambda d:d['submission_samples']['rows'][0]['cpu_ms'].__setitem__(0,3)),
 ('enclosing',lambda d:d['submission_samples']['rows'][0].update(render_ms=999,cpu_ms=[999,0,0])),
 ('batch_count',lambda d:d['submission_samples']['rows'][0].update(batches=4)),
 ('upload_bytes',lambda d:d['submission_samples']['rows'][0].update(upload_bytes=0)),
 ('row_count',lambda d:d['submission_samples']['rows'].pop()),
 ('payload_bytes',lambda d:d['memory'].update(submission_sample_buffer_bytes=0)),
 ('checksum',lambda d:d['run'].update(checksum='0')),
 ('aggregate',lambda d:d['milliseconds']['render_submit'].update(p99=999)),
]
Path('build/submission-controls').mkdir(parents=True,exist_ok=True)
checks=[]
for name,mutate in [('valid',lambda d:None),*cases]:
 with tempfile.TemporaryDirectory(dir='build/submission-controls') as tmp:
  dest=Path(tmp);current=copy.deepcopy(manifest)
  for e in current['runs']:
   for k in ['report','log']:
    (dest/e[k]).write_bytes((root/e[k]).read_bytes())
  (dest/'collect.py').write_bytes((root/'collect.py').read_bytes())
  path=dest/'split-1.json';data=json.loads(path.read_text());mutate(data)
  path.write_text(json.dumps(data))
  next(e for e in current['runs'] if e['report']==path.name)['report_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
  (dest/'manifest.json').write_text(json.dumps(current))
  p=subprocess.run(['python3',str(root/'analyze.py'),str(dest)],capture_output=True,text=True)
  assert (p.returncode==0)==(name=='valid'),(name,p.returncode,p.stdout,p.stderr)
  checks.append(dict(control=name,exit_code=p.returncode,expected_accept=name=='valid'))
(root/'analyzer-controls.json').write_text(json.dumps(checks,indent=2)+'\n')
print('Valid input accepted; ten independently corrupted records rejected')
