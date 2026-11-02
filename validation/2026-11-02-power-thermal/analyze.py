"""Validate observations and workload identity; do not decide causal attribution."""
import hashlib,json,math,sys
from pathlib import Path

def read(p):
    return json.loads(p.read_text(),parse_constant=lambda s: (_ for _ in ()).throw(ValueError(s)))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def finite(x):return type(x) in (int,float) and math.isfinite(x) and x>=0

def analyze(root):
    m=read(root/'manifest.json')
    assert m['schema']=='power_thermal_observation_v1' and 'finished_utc' in m
    assert digest(Path('benchmarks/POWER_THERMAL_OBSERVATION.md'))==m['protocol_sha256']
    assert digest(root/'collect.py')==m['collector_sha256'] and m['interval_seconds']==.5
    assert [p['name'] for p in m['phases']]==['before','cpu-reference',*[f'graphics-{i}' for i in range(1,7)],'after']
    assert m['fields'] and all(kind in ('int','text') for kind in m['fields'].values())
    summaries=[];reports=[];previous_end=0;reference=None;signature=None
    policy=read(Path('validation/2026-10-26-workload-acceptance/policy.json'))['workloads']['combined']['workload']|{'warmup_per_cycle':600}
    for p in m['phases']:
        assert finite(p['start_offset_seconds']) and finite(p['end_offset_seconds'])
        assert previous_end<=p['start_offset_seconds']<p['end_offset_seconds'];previous_end=p['end_offset_seconds']
        assert p['observations']==p['name']+'-observations.json'
        assert digest(root/p['observations'])==p['observations_sha256']
        samples=read(root/p['observations']);assert len(samples)>=2
        prev=p['start_offset_seconds'];seen={path:[] for path in m['fields']};errors={path:[] for path in m['fields']}
        for sample in samples:
            assert finite(sample['offset_seconds']) and finite(sample['read_seconds'])
            end=sample['offset_seconds']+sample['read_seconds']
            assert prev<=sample['offset_seconds']<=end<=p['end_offset_seconds'];prev=end
            assert set(sample['values'])==set(m['fields'])
            assert len(sample['host_cpu_ticks'])>=8 and all(type(x) is int and x>=0 for x in sample['host_cpu_ticks'])
            assert len(sample['load_average'])==3 and all(finite(x) for x in sample['load_average'])
            for path,item in sample['values'].items():
                assert set(item)=={'value','error'}
                if item['value'] is None:
                    e=item['error'];assert type(e) is dict and set(e)=={'type','errno'} and isinstance(e['type'],str) and e['type']
                    assert e['errno'] is None or (type(e['errno']) is int and e['errno']>0)
                    errors[path].append(e)
                else:
                    assert item['error'] is None
                    assert type(item['value']) is (int if m['fields'][path]=='int' else str)
                    seen[path].append(item['value'])
        assert finite(p['collector_cpu_seconds']) and p['collector_cpu_seconds']<=p['end_offset_seconds']-p['start_offset_seconds']
        fields={}
        for path,values in seen.items():
            unique=sorted(set(values));item=dict(valid=len(values),errors=len(errors[path]),unique=unique)
            if m['fields'][path]=='int' and values:
                item.update(minimum=min(values),maximum=max(values),median=sorted(values)[(len(values)-1)//2])
                if path.endswith('_throttle_count'):
                    assert all(v>=0 for v in values)
                    item['boundary_delta']=samples[-1]['values'][path]['value']-samples[0]['values'][path]['value'] if all(s['values'][path]['value'] is not None for s in [samples[0],samples[-1]]) else None
                    item['regressed']=any(b<a for a,b in zip(values,values[1:]))
            fields[path]=item
        summary=dict(name=p['name'],sample_count=len(samples),duration_seconds=p['end_offset_seconds']-p['start_offset_seconds'],
                     collector_cpu_seconds=p['collector_cpu_seconds'],max_read_seconds=max(s['read_seconds'] for s in samples),
                     fields=fields)
        freq='/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq'
        if freq in seen:
            summary['policy0_low_frequency_samples']=sum(v<=900000 for v in seen[freq])
            summary['policy0_frequency_samples']=len(seen[freq])
        summaries.append(summary)
        if p['command'] is None:
            assert p['exit_code'] is None and p['requested_seconds']==(20 if p['name']=='before' else 60)
            assert summary['duration_seconds']>=p['requested_seconds'];continue
        assert p['exit_code']==0 and not p['timed_out'] and p['rss_peak_bytes']>0
        assert p['report']==p['name']+'.json' and p['log']==p['name']+'.log'
        assert digest(root/p['report'])==p['report_sha256'] and digest(root/p['log'])==p['log_sha256']
        d=read(root/p['report']);r=d['run']
        assert d['build']['revision']==m['source'][:7] and d['build']['configuration']=='Release' and 'sanitizers=OFF' in d['build']['flags']
        assert d['complete'] and d['resources_stable'] and d['workload']==policy
        assert r['measured_frames']==1800 and r['completed_cycles']==3 and not r['audio_device'] and not r['capture_requested']
        if p['name']=='cpu-reference':
            assert d['mode']=='cpu';reference=d;continue
        assert reference is not None and d['mode']=='graphics'
        assert r['checksum']==reference['run']['checksum']
        assert d['totals']==dict(quads=18432000,culled=3686400,draws=5400,visible_tiles=7372800,
                                collision_candidates=reference['totals']['collision_candidates'],contacts=reference['totals']['contacts'])
        assert d['presentation']==dict(mode='immediate',swap_interval_requested=0,request_issued=True,present_calls=3600)
        q=d['gpu_queries'];assert q['supported'] and q['invalid']==0 and q['skipped']==0 and not q['failures'] and q['failures_omitted']==0
        assert q['submitted']==3600 and q['completed']+q['pending']==3600 and 0<=q['pending']<=8
        mem=d['memory']
        assert mem['peak_uploaded_textures']==mem['peak_targets']==1 and mem['peak_uploaded_texture_bytes']==16 and mem['peak_target_bytes']==8294400
        assert mem['tracked_cpu_buffer_bytes']==8072232 and mem['report_buffer_bytes']==100824
        current=(d['machine'],r['window_width'],r['window_height'])
        if signature is None:signature=current
        assert current==signature
        frame=d['frame_samples'];prep=d['preparation_samples']
        assert frame['cpu_scope_order']==['poll_collect','simulation','mixer','render_submit','preview_submit','present','frame']
        assert prep['phase_order']==['begin','tiles','sprites','end'] and prep['metric_order']==['wall_ms','thread_cpu_ms','gl_wall_ms','gl_thread_cpu_ms']
        assert len(frame['rows'])==len(prep['rows'])==1800
        values=[]
        for i,(row,part) in enumerate(zip(frame['rows'],prep['rows'])):
            tag=i//600*1200+601+i%600
            assert int(row['frame'])==int(part['frame'])==tag
            assert len(row['cpu_ms'])==7 and all(finite(x) for x in row['cpu_ms'])
            assert math.isclose(sum(row['cpu_ms'][:6]),row['cpu_ms'][6],abs_tol=1e-6,rel_tol=1e-7)
            assert len(part['phases'])==4
            for phase in part['phases']:
                assert len(phase)==4 and all(finite(x) for x in phase)
                assert phase[2]<=phase[0]+1e-6 and phase[3]<=phase[1]+1e-6
            values.append(sum(phase[1]-phase[3] for phase in part['phases']))
        median=sorted(values)[899]
        times=d['milliseconds']['frame'];assert times['samples']==1800 and all(finite(v) for v in times.values())
        frame_values=sorted(row['cpu_ms'][6] for row in frame['rows'])
        for field,idx in [('median',899),('p95',1709),('p99',1781),('max',1799)]:
            assert math.isclose(times[field],frame_values[idx],abs_tol=1e-6,rel_tol=1e-7)
        reports.append(dict(name=p['name'],preparation_cpu_median_ms=median,frame=times,rss_peak_bytes=p['rss_peak_bytes']))
    assert len(reports)==6
    return dict(schema='power_thermal_analysis_v1',source=m['source'],evidence_valid=True,
                causal_attribution='unresolved',release_accepted=False,phases=summaries,reports=reports)
if __name__=='__main__':
    root=Path(sys.argv[1]);result=analyze(root)
    (root/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS observation/workload checks; causal attribution remains unresolved')
