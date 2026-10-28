"""Declared alternating context-switch snapshots; all attempts retained without retries."""
import datetime,hashlib,json,os,subprocess,time
from pathlib import Path
root=Path('validation/2026-10-28-functions')
perf=Path('build/function-validation/perf/usr/bin/perf')
os.environ['LD_LIBRARY_PATH']=str(Path('build/function-validation/perf/usr/lib').resolve())
os.environ['PERF_EXEC_PATH']=str(Path('build/function-validation/perf/usr/lib/perf').resolve())
def digest(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def utc(): return datetime.datetime.now(datetime.timezone.utc).isoformat()
def temperatures():
 result={}
 for p in Path('/sys/class/thermal').glob('thermal_zone*/temp'):
  try: result[str(p)]=int(p.read_text())
  except (OSError,ValueError):pass
 return result
assert not subprocess.check_output(['git','diff','--name-only','HEAD'],text=True).strip()
manifest={'schema':'stress_function_collection_v1','source':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'started_utc':utc(),'executable_sha256':digest('build/release/feature_lab'),'declaration_sha256':digest('benchmarks/STRESS_FUNCTION_SAMPLING.md'),'collector_sha256':digest(__file__),'display':os.environ.get('DISPLAY'),'environment_overrides':{k:os.environ[k] for k in ['LD_LIBRARY_PATH','PERF_EXEC_PATH']},'perf_sha256':digest(perf),'runs':[]}
for repeat,modes in [(1,['baseline','sample']),(2,['sample','baseline']),(3,['baseline','sample'])]:
 for mode in modes:
  stem=f'{mode}-{repeat}';report=root/(stem+'.json');log=root/(stem+'.log')
  assert not report.exists() and not log.exists()
  args=['build/release/feature_lab','--stress','--width','1920','--height','1080','--frames','600','--warmup','60','--cycles','3','--frame-samples','--submission-samples','--preparation-samples','--presentation','immediate','--report',str(report)]
  args.append('--context-samples')
  if mode=='sample': args=[str(perf),'record','--no-inherit','-e','cpu-clock:u','-c','1000000','-m','128','--no-buildid-cache','-o',f'build/function-validation/{stem}.data','--',*args]
  entry={'mode':mode,'repeat':repeat,'command':args,'report':report.name,'log':log.name,'started_utc':utc(),'temperatures_before':temperatures()}
  print('START',stem,flush=True);start=time.monotonic()
  with log.open('x') as f:
   p=subprocess.Popen(args,stdout=f,stderr=subprocess.STDOUT)
   _,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
  entry.update(exit_code=p.returncode,seconds=time.monotonic()-start,rss_peak_bytes=usage.ru_maxrss*1024,finished_utc=utc(),temperatures_after=temperatures(),report_sha256=digest(report) if report.exists() else None,log_sha256=digest(log))
  manifest['runs'].append(entry);(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
  print('DONE',stem,p.returncode,flush=True)
manifest['finished_utc']=utc();(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
