"""Declared alternating context-switch snapshots; all attempts retained without retries."""
import datetime,hashlib,json,os,subprocess,time
from pathlib import Path
root=Path('validation/2026-10-28-context')
def digest(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def utc(): return datetime.datetime.now(datetime.timezone.utc).isoformat()
def temperatures():
 result={}
 for p in Path('/sys/class/thermal').glob('thermal_zone*/temp'):
  try: result[str(p)]=int(p.read_text())
  except (OSError,ValueError):pass
 return result
assert not subprocess.check_output(['git','diff','--name-only','HEAD'],text=True).strip()
manifest={'schema':'stress_context_collection_v1','source':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'started_utc':utc(),'executable_sha256':digest('build/release/feature_lab'),'declaration_sha256':digest('benchmarks/STRESS_CONTEXT_SWITCHES.md'),'collector_sha256':digest(__file__),'display':os.environ.get('DISPLAY'),'runs':[]}
for repeat,modes in [(1,['baseline','context']),(2,['context','baseline']),(3,['baseline','context'])]:
 for mode in modes:
  stem=f'{mode}-{repeat}';report=root/(stem+'.json');log=root/(stem+'.log')
  assert not report.exists() and not log.exists()
  args=['build/release/feature_lab','--stress','--width','1920','--height','1080','--frames','600','--warmup','60','--cycles','3','--frame-samples','--submission-samples','--preparation-samples','--presentation','immediate','--report',str(report)]
  if mode=='context': args.append('--context-samples')
  entry={'mode':mode,'repeat':repeat,'command':args,'report':report.name,'log':log.name,'started_utc':utc(),'temperatures_before':temperatures()}
  print('START',stem,flush=True);start=time.monotonic()
  with log.open('x') as f:
   p=subprocess.Popen(args,stdout=f,stderr=subprocess.STDOUT)
   _,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
  entry.update(exit_code=p.returncode,seconds=time.monotonic()-start,rss_peak_bytes=usage.ru_maxrss*1024,finished_utc=utc(),temperatures_after=temperatures(),report_sha256=digest(report) if report.exists() else None,log_sha256=digest(log))
  manifest['runs'].append(entry);(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
  print('DONE',stem,p.returncode,flush=True)
manifest['finished_utc']=utc();(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
