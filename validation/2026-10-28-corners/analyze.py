"""Validate complete traces, reconstruct aggregates, retain paired outliers."""
import hashlib,json,math,sys
from pathlib import Path
root=Path(sys.argv[1])
def read(p): return json.loads(p.read_text(),parse_constant=lambda s: (_ for _ in ()).throw(ValueError(s)))
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def close(a,b): return math.isclose(a,b,rel_tol=1e-7,abs_tol=1e-6)
def distribution(values):
 values=sorted(values);n=len(values)
 assert n
 return {'samples':n,'mean':sum(values)/n,'median':values[(n-1)*50//100],'p95':values[(n-1)*95//100],'p99':values[(n-1)*99//100],'max':values[-1]}
manifest=read(root/'manifest.json');assert 'finished_utc' in manifest
assert digest(Path('benchmarks/QUAD_CORNER_REUSE.md'))==manifest['declaration_sha256']
assert digest(root/'collect.py')==manifest['collector_sha256']
assert [(e['repeat'],e['mode']) for e in manifest['runs']]==[(1,'reference'),(1,'candidate'),(2,'candidate'),(2,'reference'),(3,'reference'),(3,'candidate')]
policy=read(Path('validation/2026-10-26-workload-acceptance/policy.json'))['workloads']['combined']['workload']
scopes=['poll_collect','simulation','mixer','render_submit','preview_submit','present','frame']
results=[];signature=None
for entry in manifest['runs']:
 assert entry['exit_code']==0
 assert digest(root/entry['report'])==entry['report_sha256'] and digest(root/entry['log'])==entry['log_sha256']
 d=read(root/entry['report']);run=d['run'];q=d['gpu_queries'];mem=d['memory'];times=d['milliseconds']
 assert d['build']['revision']==(manifest['reference_source'] if entry['mode']=='reference' else manifest['source'])[:7] and d['build']['configuration']=='Release' and 'sanitizers=OFF' in d['build']['flags']
 assert d['complete'] and d['resources_stable'] and d['workload']==policy
 assert run['completed_cycles']==3 and run['measured_frames']==1800 and run['checksum']=='3573445382734126645'
 assert not run['audio_device'] and not run['capture_requested']
 vsync=False
 assert d['presentation']=={'mode':'immediate','swap_interval_requested':int(vsync),'request_issued':True,'present_calls':1980}
 assert run['vsync_requested']==vsync and run['vsync_disable_accepted']==(not vsync)
 assert d['totals']=={'quads':18432000,'culled':3686400,'draws':5400,'visible_tiles':7372800,'collision_candidates':1396800,'contacts':0}
 assert q['supported'] and q['method']=='time_elapsed' and q['counter_bits']==64
 assert q['submitted']+q['skipped']==1980 and q['completed']+q['pending']==q['submitted'] and q['invalid']==0 and 0<=q['pending']<=8
 assert mem['peak_uploaded_textures']==mem['peak_targets']==1 and mem['peak_uploaded_texture_bytes']==16 and mem['peak_target_bytes']==8294400
 assert mem['report_buffer_bytes']==100824 and mem['tracked_cpu_buffer_bytes']==8072232
 current=(d['machine'],{k:v for k,v in d['build'].items() if k!='revision'},d['workload'],d['totals'],run['window_width'],run['window_height'])
 if signature is None:signature=current
 assert current==signature
 result={'mode':entry['mode'],'repeat':entry['repeat'],'frame':times['frame'],'present':times['present'],'gpu_render':times['gpu_render'],'rss_peak_bytes':entry['rss_peak_bytes']}
 trace=d['frame_samples'];rows=trace['rows']
 assert trace['schema']=='stress_frame_samples_v1' and trace['cpu_scope_order']==scopes and len(rows)==1800
 assert mem['frame_sample_buffer_bytes']==1800*88
 completed=[];missing=0
 for i,row in enumerate(rows):
  tag=int(row['frame']);assert tag==i//600*660+61+i%600
  cpu=row['cpu_ms'];assert len(cpu)==7 and all(type(v) in (int,float) and math.isfinite(v) and v>=0 for v in cpu)
  assert close(sum(cpu[:6]),cpu[6]),(tag,cpu)
  if row['gpu_status']=='completed':
   assert type(row['gpu_ms']) in (int,float) and math.isfinite(row['gpu_ms']) and row['gpu_ms']>=0
   assert tag<=int(row['gpu_collected_after'])<=1980
   completed.append(row['gpu_ms'])
  else:
   assert row['gpu_status'] in ('uncollected','not_submitted') and row['gpu_ms'] is None and row['gpu_collected_after'] is None
   missing+=1
 for j,scope in enumerate(scopes[1:],1):
  rebuilt=distribution([row['cpu_ms'][j] for row in rows]);reported=times[scope]
  assert all(close(v,reported[k]) for k,v in rebuilt.items()),(scope,rebuilt,reported)
 if completed:
  rebuilt=distribution(completed);assert all(close(v,times['gpu_render'][k]) for k,v in rebuilt.items())
 else: assert times['gpu_render']['samples']==0
 assert sum(row['gpu_status']=='uncollected' for row in rows)==q['pending']
 assert sum(row['gpu_status']=='not_submitted' for row in rows)<=q['skipped']
 assert len(completed)==times['gpu_render']['samples']
 over=[row for row in rows if row['cpu_ms'][6]>16.7]
 largest={s:0 for s in scopes[:6]}
 for row in over: largest[scopes[max(range(6),key=lambda i:row['cpu_ms'][i])]]+=1
 worst=[]
 for row in sorted(rows,key=lambda row:row['cpu_ms'][6],reverse=True)[:10]:
  worst.append({'frame':row['frame'],'cpu_ms':dict(zip(scopes,row['cpu_ms'])),'largest_cpu_interval':scopes[max(range(6),key=lambda i:row['cpu_ms'][i])], 'gpu_ms':row['gpu_ms'],'gpu_collected_after':row['gpu_collected_after'],'collection_age_frames':int(row['gpu_collected_after'])-int(row['frame']) if row['gpu_collected_after'] else None})
 split=d['submission_samples'];nested=split['rows']
 assert split['schema']=='stress_submission_samples_v1'
 assert split['cpu_scope_order']==['prepare_other','gl_setup','gl_upload_draw'] and len(nested)==len(rows)
 assert mem['submission_sample_buffer_bytes']==1800*56
 for row,part in zip(rows,nested):
  assert row['frame']==part['frame']
  v=part['cpu_ms'];assert len(v)==3 and all(type(x) in (int,float) and math.isfinite(x) and x>=0 for x in v)
  assert math.isfinite(part['render_ms']) and close(sum(v),part['render_ms'])
  assert part['render_ms']<=row['cpu_ms'][3]+1e-6
  assert part['batches']==3 and part['upload_bytes']==10240*6*32
 def paired(i):
  row=rows[i];part=nested[i]
  return {'frame':row['frame'],'frame_ms':row['cpu_ms'][6],'submit_ms':row['cpu_ms'][3],
          'present_ms':row['cpu_ms'][5], 'gpu_ms':row['gpu_ms'],
          'cpu_ms':dict(zip(split['cpu_scope_order'],part['cpu_ms'])),
          'query_other_ms':row['cpu_ms'][3]-part['render_ms'],
          'largest_render_interval':split['cpu_scope_order'][max(range(3),key=lambda j:part['cpu_ms'][j])]}
 result['submission']={
  'distributions':{name:distribution([part['cpu_ms'][j] for part in nested]) for j,name in enumerate(split['cpu_scope_order'])},
  'largest_ten_submissions':[paired(i) for i in sorted(range(len(rows)),key=lambda i:rows[i]['cpu_ms'][3],reverse=True)[:10]],
  'submission_dominant_slow_frames':[paired(i) for i,row in enumerate(rows) if row['cpu_ms'][6]>16.7 and max(range(6),key=lambda j:row['cpu_ms'][j])==3],
  'buffer_bytes':mem['submission_sample_buffer_bytes']}
 profile=d['preparation_samples'];phases=profile['rows']
 names=['begin','tiles','sprites','end']
 assert profile['schema']=='stress_preparation_samples_v1'
 assert profile['clock']=='CLOCK_THREAD_CPUTIME_ID' and profile['resolution_ns']>0
 assert profile['phase_order']==names
 assert profile['metric_order']==['wall_ms','thread_cpu_ms','gl_wall_ms','gl_thread_cpu_ms']
 assert len(phases)==len(rows) and mem['preparation_sample_buffer_bytes']==1800*136
 derived=[]
 for row,part,phase in zip(rows,nested,phases):
  assert row['frame']==phase['frame'] and len(phase['phases'])==4
  for p in phase['phases']:
   assert len(p)==4 and all(type(v) in (int,float) and math.isfinite(v) and v>=0 for v in p)
   assert p[2]<=p[0]+1e-6 and p[3]<=p[1]+1e-6
  values=phase['phases'];wall=sum(p[0] for p in values)
  assert wall<=part['render_ms']+1e-6
  assert close(sum(p[2] for p in values),sum(part['cpu_ms'][1:]))
  prep_wall=sum(p[0]-p[2] for p in values);prep_cpu=sum(p[1]-p[3] for p in values)
  derived.append({'frame':row['frame'],'frame_ms':row['cpu_ms'][6],'submission_ms':row['cpu_ms'][3],
   'present_ms':row['cpu_ms'][5], 'gpu_ms':row['gpu_ms'],
   'phase_preparation_wall_ms':prep_wall,'phase_preparation_cpu_ms':prep_cpu,
   'wall_minus_cpu_ms':prep_wall-prep_cpu,
   'cpu_to_wall_ratio':prep_cpu/prep_wall if prep_wall>0 else None,
   'unmeasured_wall_ms':part['render_ms']-wall,
   'phases':{name:{'wall_ms':p[0],'thread_cpu_ms':p[1],'gl_wall_ms':p[2],'gl_thread_cpu_ms':p[3],
      'preparation_wall_ms':p[0]-p[2],'preparation_cpu_ms':p[1]-p[3],
      'preparation_wall_minus_cpu_ms':(p[0]-p[2])-(p[1]-p[3])} for name,p in zip(names,values)}})
 result['preparation']={
  'clock_resolution_ns':profile['resolution_ns'], 'buffer_bytes':mem['preparation_sample_buffer_bytes'],
  'distributions':{k:distribution([r[k] for r in derived]) for k in ['phase_preparation_wall_ms','phase_preparation_cpu_ms','wall_minus_cpu_ms','unmeasured_wall_ms']},
  'phase_distributions':{name:{k:distribution([r['phases'][name][k] for r in derived]) for k in ['preparation_wall_ms','preparation_cpu_ms','preparation_wall_minus_cpu_ms']} for name in names},
  'negative_gap_rows':sum(r['wall_minus_cpu_ms']<0 for r in derived),
  'largest_ten_submissions':[derived[i] for i in sorted(range(len(rows)),key=lambda i:rows[i]['cpu_ms'][3],reverse=True)[:10]],
  'submission_dominant_slow_frames':[derived[i] for i,row in enumerate(rows) if row['cpu_ms'][6]>16.7 and max(range(6),key=lambda j:row['cpu_ms'][j])==3]}
 if entry['mode'] in ('reference','candidate'):
  ctx=d['context_samples'];counter_rows=ctx['rows']
  assert ctx['schema']=='stress_context_samples_v1' and ctx['source']=='getrusage(RUSAGE_THREAD)'
  assert ctx['phase_order']==names and ctx['counter_order']==['voluntary','involuntary']
  assert len(counter_rows)==len(rows) and mem['context_sample_buffer_bytes']==1800*136
  def integer(value):
   assert type(value) is str and value.isascii() and value.isdigit() and len(value)<=20
   number=int(value);assert str(number)==value and 0<=number<=2**64-1
   return number
  def category(v,i):return 'both' if v and i else 'voluntary_only' if v else 'involuntary_only' if i else 'none'
  counters=[];previous=[0,0]
  for row,p,context in zip(rows,derived,counter_rows):
   assert context['frame']==row['frame'] and len(context['phases'])==4
   phase_counts={};v_total=i_total=0
   for name,snapshot in zip(names,context['phases']):
    assert len(snapshot['before'])==len(snapshot['after'])==2
    before=list(map(integer,snapshot['before']));after=list(map(integer,snapshot['after']))
    assert all(previous[j]<=before[j]<=after[j] for j in range(2))
    v,i=[after[j]-before[j] for j in range(2)];previous=after
    phase_counts[name]={'voluntary':v,'involuntary':i,'category':category(v,i)}
    v_total+=v;i_total+=i
   counters.append(p|{'voluntary':v_total,'involuntary':i_total,'category':category(v_total,i_total),
    'phase_counters':phase_counts,
    'whole_phase_gap_ms':sum(x['wall_ms']-x['thread_cpu_ms'] for x in p['phases'].values())})
  def summarize(items):
   return {'samples':len(items),'voluntary_switches':sum(x['voluntary'] for x in items),
    'involuntary_switches':sum(x['involuntary'] for x in items),
    'preparation_gap_over_1_ms':sum(x['wall_minus_cpu_ms']>1 for x in items),
    'preparation_gap_ms':distribution([x['wall_minus_cpu_ms'] for x in items]) if items else None,
    'whole_phase_gap_ms':distribution([x['whole_phase_gap_ms'] for x in items]) if items else None}
  categories=['none','voluntary_only','involuntary_only','both']
  phase_groups={}
  for name in names:
   items=[]
   for p in counters:
    t=p['phases'][name];c=p['phase_counters'][name]
    items.append(c|{'wall_minus_cpu_ms':t['preparation_wall_minus_cpu_ms'],
                  'whole_phase_gap_ms':t['wall_ms']-t['thread_cpu_ms']})
   phase_groups[name]={c:summarize([p for p in items if p['category']==c]) for c in categories}
  result['context']={'buffer_bytes':mem['context_sample_buffer_bytes'],'diagnostic_gap_threshold_ms':1,
   'all_frames':summarize(counters),
   'frame_groups':{c:summarize([p for p in counters if p['category']==c]) for c in categories},
   'phase_groups':phase_groups,
   'largest_ten_submissions':[counters[i] for i in sorted(range(len(rows)),key=lambda i:rows[i]['cpu_ms'][3],reverse=True)[:10]],
   'submission_dominant_slow_frames':[counters[i] for i,row in enumerate(rows) if row['cpu_ms'][6]>16.7 and max(range(6),key=lambda j:row['cpu_ms'][j])==3]}
 else:
  assert 'context_samples' not in d and 'context_sample_buffer_bytes' not in mem
 result.update(aggregates_reconstructed=True,cpu_partition_verified=True,over_16_7_ms=len(over),over_16_7_percent=len(over)/18,largest_interval_on_slow_frames=largest,worst_ten=worst,missing_gpu_rows=missing,frame_sample_buffer_bytes=mem['frame_sample_buffer_bytes'])
 results.append(result)
out={'schema':'quad_corner_analysis_v1','source':manifest['source'],'all_attempts_valid':True,'requests_issued':True,'physical_synchronization_verified':False,'attempts':results,'release_accepted':False}
pairs=[]
for repeat in (1,2,3):
 a,b=[next(r for r in results if r['mode']==mode and r['repeat']==repeat) for mode in ('reference','candidate')]
 key='phase_preparation_cpu_ms'
 old=a['preparation']['distributions'][key]['median'];new=b['preparation']['distributions'][key]['median']
 pairs.append({'repeat':repeat,'reference_cpu_median_ms':old,'candidate_cpu_median_ms':new,'reduction_percent':100*(old-new)/old})
out.update(pairs=pairs,cpu_gate_passed=all(p['reduction_percent']>=5 for p in pairs),adoption_requires_correctness=True)
(root/'analysis.json').write_text(json.dumps(out,indent=2)+'\n')
for r in results: print(r['mode'],r['repeat'],'p99',r['frame']['p99'],'max',r['frame']['max'],'slow',r.get('over_16_7_ms'),'largest',r.get('largest_interval_on_slow_frames'))
