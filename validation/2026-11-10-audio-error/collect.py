"""Run the predeclared silent observation once; preserve outputs and measured failures."""
import datetime,hashlib,json,os,subprocess,time
from pathlib import Path
root=Path('validation/2026-11-10-audio-error');work=Path('build/audio-error-refresh')
scratch=Path((work/'scratch.txt').read_text().strip());os.environ['TMPDIR']=str(scratch/'compiler-tmp')
assert not (root/'collection-commands.json').exists()
assert not subprocess.check_output(['git','diff','--name-only','HEAD'],text=True).strip()
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip();commands=[]
def run(name,args,allowed=(0,),env=None,timeout=60):
 print('START',name,flush=True);start=time.monotonic();stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
 with (root/(name+'.log')).open('x') as log:
  try:p=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,env=env,timeout=timeout);code=p.returncode
  except subprocess.TimeoutExpired:code=124
 commands.append(dict(name=name,command=args,source=source,started_utc=stamp,seconds=time.monotonic()-start,exit_code=code,timeout=timeout,environment={'TMPDIR':os.environ['TMPDIR'],'LD_PRELOAD':(env or os.environ).get('LD_PRELOAD')}))
 (root/'collection-commands.json').write_text(json.dumps(commands,indent=2)+'\n');print('DONE',name,code,flush=True)
 if code not in allowed:print((root/(name+'.log')).read_text()[-4000:]);raise SystemExit(code)
run('environment',['bash','-c','uname -a; c++ --version; pkg-config --modversion alsa; aplay -L; cat /proc/asound/cards; cat /usr/share/alsa/alsa.conf.d/99-pipewire-default.conf'])
run('trace-build',['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',str(root/'trace.cpp'),'-lasound','-ldl','-o',str(work/'alsa-trace.so')])
run('silent-build',['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-Iengine/include',str(root/'silent.cpp'),'build/release/libengine_platform.a','build/release/libengine_core.a','-lasound','-lX11','-lGLX','-lOpenGL','-pthread','-o',str(work/'silent')])
paths=[work/'silent',work/'alsa-trace.so',Path('build/release/libengine_platform.a'),Path('build/release/libengine_core.a'),root/'trace.cpp',root/'silent.cpp',root/'PROTOCOL.md']
(root/'collection-hashes.json').write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},indent=2)+'\n')
env=dict(os.environ,LD_PRELOAD=str((work/'alsa-trace.so').resolve()))
for i in [1,2]:run('silent-'+str(i),[str((work/'silent').resolve())],allowed=(0,2),env=env,timeout=140)
