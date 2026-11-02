"""Prepare, observe once, then validate; no automatic retries or policy changes."""
import datetime,json,os,subprocess,time,xml.etree.ElementTree as ET
from pathlib import Path
root=Path('validation/2026-11-02-power-thermal')
scratch=Path(Path('build/power-thermal-refresh/scratch.txt').read_text().strip())
os.environ['PATH']=str(Path.cwd()/'.tools/bin')+':'+os.environ['PATH']
os.environ['TMPDIR']=str(scratch/'compiler-tmp');os.environ['PYTHONDONTWRITEBYTECODE']='1'
assert not (root/'commands.json').exists()
assert not subprocess.check_output(['git','diff','--name-only','HEAD'],text=True).strip()
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip();commands=[]
def run(name,args,allowed=(0,)):
    print('START',name,flush=True);start=time.monotonic();stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
    with (root/(name+'.log')).open('x') as f:p=subprocess.run(args,stdout=f,stderr=subprocess.STDOUT)
    commands.append(dict(name=name,command=args,source=source,started_utc=stamp,seconds=time.monotonic()-start,exit_code=p.returncode,
                         cwd=str(Path.cwd()),environment={k:os.environ.get(k) for k in ['PATH','TMPDIR','DISPLAY','PYTHONDONTWRITEBYTECODE']}))
    (root/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');print('DONE',name,p.returncode,flush=True)
    if p.returncode not in allowed:print((root/(name+'.log')).read_text()[-3000:]);raise SystemExit(p.returncode)
def host(phase):
    run(phase+'-profile',['powerprofilesctl','list'])
    run(phase+'-services',['systemctl','is-active','power-profiles-daemon','thermald','tlp','tuned'],(0,3,4))
    run(phase+'-thermal-journal',['journalctl','-b','-u','thermald','-u','power-profiles-daemon','--no-pager','-n','100','-o','short-iso'])
    run(phase+'-kernel-journal',['journalctl','-k','-b','--no-pager','-o','short-iso','--grep=(thermal|throttl|power limit|pstate|PROCHOT)','-n','100'],(0,1))
run('environment',['bash','-c','uname -a; c++ --version; cmake --version; glxinfo -B'])
run('configure',['cmake','--preset','release','-DENGINE_PACKAGE_TEST_ROOT='+str(scratch/'packages')])
run('build',['cmake','--build','--preset','release','--target','feature_lab','stress_bench','stress_tests','stress_graphics_tests','-j2'])
run('tests',['ctest','--preset','release','-R','^(stress_cpu|stress_graphics|stress_frame_samples)$','--output-on-failure','--output-junit',str((root/'checks.xml').resolve())])
(root/'tests-full-output.log').write_bytes(Path('build/release/Testing/Temporary/LastTest.log').read_bytes())
x=ET.parse(root/'checks.xml').getroot();assert int(x.attrib['tests'])==3 and int(x.attrib['failures'])==int(x.attrib['skipped'])==0
assert not subprocess.check_output(['git','diff','--name-only','3adc6d2','HEAD','--','engine','examples','tests','CMakeLists.txt'],text=True).strip()
host('before');run('collection',['python3',str(root/'collect.py')]);host('after')
run('analysis',['python3',str(root/'analyze.py'),str(root)])
run('controls',['python3',str(root/'controls.py')])
