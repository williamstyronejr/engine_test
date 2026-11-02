"""Positive and corrupt-input controls on isolated copies; never alter collected data."""
import copy,importlib.util,json,hashlib,tempfile
from pathlib import Path
ROOT=Path('validation/2026-11-02-power-thermal')
spec=importlib.util.spec_from_file_location('power_analysis',ROOT/'analyze.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
m=json.loads((ROOT/'manifest.json').read_text())
cases=[('valid','none',lambda d:None,True),
       ('unavailable_sensor','sensor',lambda d:[s['values'].__setitem__(next(iter(s['values'])),dict(value=None,error=dict(type='PermissionError',errno=13))) for s in d],True),
       ('phase_order','manifest',lambda d:d['phases'].reverse(),False),
       ('unfinished','manifest',lambda d:d.pop('finished_utc'),False),
       ('wrong_hash','manifest',lambda d:d['phases'][2].update(report_sha256='0'*64),False),
       ('time_order','sensor',lambda d:d[1].update(offset_seconds=-1),False),
       ('missing_field','sensor',lambda d:d[0]['values'].pop(next(iter(d[0]['values']))),False),
       ('missing_error','sensor',lambda d:d[0]['values'].__setitem__(next(iter(d[0]['values'])),dict(value=None,error=None)),False),
       ('wrong_type','sensor',lambda d:d[0]['values'].__setitem__('/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq',dict(value='800000',error=None)),False),
       ('source','report',lambda d:d['build'].update(revision='wrong'),False),
       ('checksum','report',lambda d:d['run'].update(checksum='0'),False),
       ('count','report',lambda d:d['run'].update(measured_frames=1),False),
       ('partition','report',lambda d:d['frame_samples']['rows'][0]['cpu_ms'].__setitem__(0,1000),False),
       ('nesting','report',lambda d:d['preparation_samples']['rows'][0]['phases'][0].__setitem__(3,1000),False),
       ('negative_time','report',lambda d:d['frame_samples']['rows'][0]['cpu_ms'].__setitem__(0,-1),False),
       ('missing_trace','report',lambda d:d['preparation_samples']['rows'].pop(),False),
       ('gpu_error','report',lambda d:d['gpu_queries'].update(invalid=1),False),
       ('presentation','report',lambda d:d['presentation'].update(present_calls=1),False),
       ('resource_bytes','report',lambda d:d['memory'].update(peak_target_bytes=1),False)]
checks=[]
for name,kind,mutate,expected in cases:
    with tempfile.TemporaryDirectory(prefix='power-control-') as tmp:
        dest=Path(tmp);current=copy.deepcopy(m)
        for p in ROOT.iterdir():
            if p.is_file() and p.name not in ('manifest.json','analysis.json','controls.json'):
                (dest/p.name).symlink_to(p.resolve())
        if kind=='manifest':mutate(current)
        elif kind in ('sensor','report'):
            entry=current['phases'][2];key='observations' if kind=='sensor' else 'report';path=dest/entry[key]
            data=json.loads(path.read_text());mutate(data);path.unlink();path.write_text(json.dumps(data))
            entry[key+'_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
        (dest/'manifest.json').write_text(json.dumps(current))
        accepted=True
        try:module.analyze(dest)
        except (AssertionError,KeyError,ValueError,TypeError):accepted=False
        assert accepted==expected,(name,accepted,expected)
        checks.append(dict(name=name,expected_accept=expected,accepted=accepted))
(ROOT/'controls.json').write_text(json.dumps(checks,indent=2)+'\n')
print('PASS',sum(x['accepted'] for x in checks),'valid controls;',sum(not x['accepted'] for x in checks),'corrupt controls rejected')
