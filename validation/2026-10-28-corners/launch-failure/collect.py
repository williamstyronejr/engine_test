"""Declared alternating quad-corner binaries; all attempts retained without retries."""
import datetime,hashlib,json,os,subprocess,sys,time
from pathlib import Path
root=Path(sys.argv[3]) if len(sys.argv)>3 else Path('validation/2026-10-28-corners')
binaries=dict(zip(['reference','candidate'],sys.argv[1:3]));assert len(binaries)==2
reference=json.loads((root/'reference-build.json').read_text())
def digest(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def utc(): return datetime.datetime.now(datetime.timezone.utc).isoformat()
def temperatures():
 result={}
 for p in Path('/sys/class/thermal').glob('thermal_zone*/temp'):
  try: result[str(p)]=int(p.read_text())
  except (OSError,ValueError):pass
 return result
assert not subprocess.check_output(['git','diff','--name-only','HEAD'],text=True).strip()
manifest={'schema':'quad_corner_collection_v1','source':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'started_utc':utc(),'reference_source':reference['source'],'executable_sha256':{k:digest(v) for k,v in binaries.items()},'declaration_sha256':digest('benchmarks/QUAD_CORNER_REUSE.md'),'collector_sha256':digest(__file__),'display':os.environ.get('DISPLAY'),'runs':[]}
assert manifest['executable_sha256']['reference']==reference['executable_sha256']
for repeat,modes in [(1,['reference','candidate']),(2,['candidate','reference']),(3,['reference','candidate'])]:
 for mode in modes:
  stem=f'{mode}-{repeat}';report=root/(stem+'.json');log=root/(stem+'.log')
  assert not report.exists() and not log.exists()
  args=[binaries[mode],'--stress','--width','1920','--height','1080','--frames','600','--warmup','60','--cycles','3','--frame-samples','--submission-samples','--preparation-samples','--presentation','immediate','--report',str(report)]
  args += ['--context-samples','--assets',str(Path('assets').resolve())]
  entry={'mode':mode,'repeat':repeat,'command':args,'report':report.name,'log':log.name,'started_utc':utc(),'temperatures_before':temperatures()}
  print('START',stem,flush=True);start=time.monotonic()
  with log.open('x') as f:
   p=subprocess.Popen(args,stdout=f,stderr=subprocess.STDOUT)
   _,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
  entry.update(exit_code=p.returncode,seconds=time.monotonic()-start,rss_peak_bytes=usage.ru_maxrss*1024,finished_utc=utc(),temperatures_after=temperatures(),report_sha256=digest(report) if report.exists() else None,log_sha256=digest(log))
  manifest['runs'].append(entry);(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
  print('DONE',stem,p.returncode,flush=True)
manifest['finished_utc']=utc();(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
