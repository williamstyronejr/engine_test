"""Export the declared perf runs before rebuilding the measured executable."""
import base64,hashlib,json,os,subprocess,zlib
from pathlib import Path
from profile_analysis import analyze
root=Path(__file__).resolve().parent;work=Path('build/function-validation');perf=work/'perf/usr/bin/perf'
m=json.loads((root/'manifest.json').read_text());env=os.environ|m['environment_overrides']
exe=Path('build/release/feature_lab').resolve()
def sha(data):return hashlib.sha256(data).hexdigest()
assert sha(exe.read_bytes())==m['executable_sha256']
records=[];profiles=[]
def run(name,args):
 p=subprocess.run(args,env=env,capture_output=True,text=True)
 (root/(name+'.log')).write_text(p.stdout)
 if p.stderr:(root/(name+'-stderr.log')).write_text(p.stderr)
 records.append(dict(command=args,exit_code=p.returncode,stdout=name+'.log',stderr=p.stderr))
 assert p.returncode==0,(args,p.stderr)
 return p.stdout
run('perf-version',[str(perf),'--version'])
run('executable-symbols',['nm','-n','-S','-C',str(exe)])
for repeat in (1,2,3):
 stem=f'sample-{repeat}';data=work/(stem+'.data');raw=data.read_bytes()
 encoded=dict(schema='raw_perf_zlib_base64_v1',bytes=len(raw),sha256=sha(raw),data=base64.b64encode(zlib.compress(raw)).decode())
 assert zlib.decompress(base64.b64decode(encoded['data']))==raw
 (root/(stem+'-raw.json')).write_text(json.dumps(encoded)+'\n')
 attrs=run(stem+'-events',[str(perf),'evlist','-v','-i',str(data)])
 assert 'inherit: 1' not in attrs and 'exclude_kernel: 1' in attrs and 'sample_period, sample_freq }: 1000000' in attrs and 'PERF_COUNT_SW_CPU_CLOCK' in attrs
 samples=run(stem+'-samples',[str(perf),'script','-F','pid,tid,ip,sym,dso','-i',str(data)])
 dump=subprocess.run([str(perf),'report','-D','-i',str(data)],env=env,capture_output=True,text=True);assert dump.returncode==0
 stats=dump.stdout.split('Aggregated stats:\n',1)[1]
 (root/(stem+'-records.log')).write_text(stats)
 records.append(dict(command=[str(perf),'report','-D','-i',str(data)],exit_code=dump.returncode,retained='Aggregated stats suffix',stdout=stem+'-records.log',stderr=dump.stderr))
 run(stem+'-functions',[str(perf),'report','--stdio','--no-children','-n','--percent-limit','0','-i',str(data)])
 run(stem+'-buildids',[str(perf),'buildid-list','-i',str(data)])
 profile=analyze(samples,stats,str(exe));profile.update(repeat=repeat,raw_sha256=sha(raw));profiles.append(profile)
(root/'profile-commands.json').write_text(json.dumps(records,indent=2)+'\n')
(root/'profiles.json').write_text(json.dumps(profiles,indent=2)+'\n')
timing=json.loads((root/'analysis.json').read_text());pairs=[]
for repeat in (1,2,3):
 a,b=[next(x for x in timing['attempts'] if x['repeat']==repeat and x['mode']==mode) for mode in ('baseline','sample')]
 pairs.append(dict(repeat=repeat,frame_median_change_percent=100*(b['frame']['median']/a['frame']['median']-1),frame_p99_change_percent=100*(b['frame']['p99']/a['frame']['p99']-1),preparation_cpu_median_change_percent=100*(b['preparation']['distributions']['phase_preparation_cpu_ms']['median']/a['preparation']['distributions']['phase_preparation_cpu_ms']['median']-1)))
(root/'overhead.json').write_text(json.dumps(pairs,indent=2)+'\n')
print('Exported three valid single-thread profiles and paired timing changes')
