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
assert [(e['repeat'],e['mode']) for e in manifest['runs']]==[(1,'immediate'),(1,'vsync'),(2,'vsync'),(2,'immediate'),(3,'immediate'),(3,'vsync')]
policy=read(Path('validation/2026-10-26-workload-acceptance/policy.json'))['workloads']['combined']['workload']
scopes=['poll_collect','simulation','mixer','render_submit','preview_submit','present','frame']
results=[];signature=None
for entry in manifest['runs']:
 assert entry['exit_code']==0
 assert digest(root/entry['report'])==entry['report_sha256'] and digest(root/entry['log'])==entry['log_sha256']
 d=read(root/entry['report']);run=d['run'];q=d['gpu_queries'];mem=d['memory'];times=d['milliseconds']
 assert d['build']['revision']==manifest['source'][:7] and d['build']['configuration']=='Release' and 'sanitizers=OFF' in d['build']['flags']
 assert d['complete'] and d['resources_stable'] and d['workload']==policy
 assert run['completed_cycles']==3 and run['measured_frames']==1800 and run['checksum']=='3573445382734126645'
 assert not run['audio_device'] and not run['capture_requested']
 vsync=entry['mode']=='vsync'
 assert d['presentation']=={'mode':entry['mode'],'swap_interval_requested':int(vsync),'request_issued':True,'present_calls':1980}
 assert run['vsync_requested']==vsync and run['vsync_disable_accepted']==(not vsync)
 assert d['totals']=={'quads':18432000,'culled':3686400,'draws':5400,'visible_tiles':7372800,'collision_candidates':1396800,'contacts':0}
 assert q['supported'] and q['method']=='time_elapsed' and q['counter_bits']==64
 assert q['submitted']+q['skipped']==1980 and q['completed']+q['pending']==q['submitted'] and q['invalid']==0 and 0<=q['pending']<=8
 assert mem['peak_uploaded_textures']==mem['peak_targets']==1 and mem['peak_uploaded_texture_bytes']==16 and mem['peak_target_bytes']==8294400
 assert mem['report_buffer_bytes']==100824 and mem['tracked_cpu_buffer_bytes']==8072232
 current=(d['machine'],d['build'],d['workload'],d['totals'],run['window_width'],run['window_height'])
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
 rebuilt=distribution(completed);assert all(close(v,times['gpu_render'][k]) for k,v in rebuilt.items())
 assert sum(row['gpu_status']=='uncollected' for row in rows)==q['pending']
 assert sum(row['gpu_status']=='not_submitted' for row in rows)<=q['skipped']
 assert len(completed)==times['gpu_render']['samples']
 over=[row for row in rows if row['cpu_ms'][6]>16.7]
 largest={s:0 for s in scopes[:6]}
 for row in over: largest[scopes[max(range(6),key=lambda i:row['cpu_ms'][i])]]+=1
 worst=[]
 for row in sorted(rows,key=lambda row:row['cpu_ms'][6],reverse=True)[:10]:
  worst.append({'frame':row['frame'],'cpu_ms':dict(zip(scopes,row['cpu_ms'])),'largest_cpu_interval':scopes[max(range(6),key=lambda i:row['cpu_ms'][i])], 'gpu_ms':row['gpu_ms'],'gpu_collected_after':row['gpu_collected_after'],'collection_age_frames':int(row['gpu_collected_after'])-int(row['frame']) if row['gpu_collected_after'] else None})
 result.update(aggregates_reconstructed=True,cpu_partition_verified=True,over_16_7_ms=len(over),over_16_7_percent=len(over)/18,largest_interval_on_slow_frames=largest,worst_ten=worst,missing_gpu_rows=missing,frame_sample_buffer_bytes=mem['frame_sample_buffer_bytes'])
 results.append(result)
out={'schema':'stress_presentation_analysis_v1','source':manifest['source'],'all_attempts_valid':True,'requests_issued':True,'physical_synchronization_verified':False,'attempts':results,'release_accepted':False}
(root/'analysis.json').write_text(json.dumps(out,indent=2)+'\n')
for r in results: print(r['mode'],r['repeat'],'p99',r['frame']['p99'],'max',r['frame']['max'],'slow',r.get('over_16_7_ms'),'largest',r.get('largest_interval_on_slow_frames'))
