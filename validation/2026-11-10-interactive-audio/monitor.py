import datetime,json,os,subprocess,time
from pathlib import Path
work=Path(__file__).resolve().parent;run=Path((work/'active.txt').read_text().strip());meta=json.loads((run/'prepared.json').read_text());start=datetime.datetime.now(datetime.timezone.utc);env=dict(os.environ,LD_PRELOAD=meta['LD_PRELOAD']);command=meta['command']
p=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,text=True,bufsize=1)
record=dict(pid=p.pid,command=command,source=meta['source'],started_utc=start.isoformat(),started_monotonic_ns=time.monotonic_ns(),status='running')
(run/'running.json').write_text(json.dumps(record,indent=2)+'\n')
with (run/'game.log').open('x') as raw,(run/'timed.log').open('x') as timed:
 for line in p.stdout:
  raw.write(line);raw.flush();timed.write(datetime.datetime.now(datetime.timezone.utc).isoformat()+' '+line);timed.flush()
code=p.wait();record.update(status='exited',exit_code=code,ended_utc=datetime.datetime.now(datetime.timezone.utc).isoformat());(run/'completion.json').write_text(json.dumps(record,indent=2)+'\n')
args=['journalctl','--user','-u','pipewire.service','-u','pipewire-pulse.service','-u','wireplumber.service','--since',start.strftime('%Y-%m-%d %H:%M:%S UTC'),'--until',datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC'),'--no-pager','-o','short-iso']
try:
 result=subprocess.run(args,capture_output=True,text=True,timeout=15);(run/'audio-services.log').write_text(result.stdout+result.stderr);(run/'journal-command.json').write_text(json.dumps(dict(command=args,exit_code=result.returncode),indent=2)+'\n')
except subprocess.TimeoutExpired:
 (run/'audio-services.log').write_text('Journal capture timed out after 15 seconds.\n')
