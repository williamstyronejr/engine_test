"""Complete current native runtime suites, sequentially; retain every attempt."""
import datetime,json,os,subprocess,time,xml.etree.ElementTree as ET
from pathlib import Path
root=Path('validation/2026-11-09-runtime')
scratch=Path(Path('build/runtime-qualification-refresh/scratch.txt').read_text().strip())
os.environ['PATH']=str(Path.cwd()/'.tools/bin')+':'+os.environ['PATH']
os.environ['TMPDIR']=str(scratch/'compiler-tmp')
assert not (root/'commands.json').exists()
assert not subprocess.check_output(['git','diff','--name-only','HEAD'],text=True).strip()
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
commands=[];checks=[]
def run(name,args,allowed=(0,)):
    stamp=datetime.datetime.now(datetime.timezone.utc).isoformat();start=time.monotonic()
    print('START',name,flush=True)
    with (root/(name+'.log')).open('x') as log:p=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT)
    commands.append(dict(name=name,command=args,source=source,cwd=str(Path.cwd()),started_utc=stamp,
                         seconds=time.monotonic()-start,exit_code=p.returncode,
                         environment={k:os.environ.get(k) for k in ['PATH','TMPDIR','DISPLAY','ASAN_OPTIONS','UBSAN_OPTIONS']}))
    (root/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    print('DONE',name,p.returncode,flush=True)
    if p.returncode not in allowed:
        print((root/(name+'.log')).read_text()[-5000:]);raise SystemExit(p.returncode)
    return p.returncode
run('environment',['bash','-c','uname -a; c++ --version; clang++ --version; cmake --version; ninja --version; glxinfo -B'])
for config in ['debug','release','clang']:
    assert not subprocess.check_output(['git','diff','--name-only','HEAD'],text=True).strip()
    run(config+'-configure',['cmake','--preset',config,'-DENGINE_PACKAGE_TEST_ROOT='+str(scratch/'packages')])
    run(config+'-build',['cmake','--build','--preset',config,'-j2'])
    build=Path('build')/config
    raw=subprocess.check_output(['ctest','--test-dir',str(build),'--show-only=json-v1','-LE','packaging'],text=True)
    inventory=json.loads(raw);(root/(config+'-inventory.json')).write_text(raw)
    assert len(inventory['tests'])==69
    names=sorted(t['name'] for t in inventory['tests']);assert len(set(names))==69
    labels={}
    for t in inventory['tests']:
        group=next(p['value'] for p in t['properties'] if p['name']=='LABELS')
        assert len(group)==1 and group[0] in ['cpu','hardware','platform'];labels[group[0]]=labels.get(group[0],0)+1
    assert labels=={'cpu':36,'hardware':29,'platform':4}
    if checks:assert names==checks[0]['groups']
    xml=root/(config+'-tests.xml')
    code=run(config+'-tests',['ctest','--test-dir',str(build),'-LE','packaging','--parallel','1',
                             '--output-on-failure','--output-junit',str(xml.resolve())],(0,8))
    (root/(config+'-full-output.log')).write_bytes((build/'Testing/Temporary/LastTest.log').read_bytes())
    suite=ET.parse(xml).getroot()
    assert sorted(t.attrib['name'] for t in suite.findall('testcase'))==names
    result=dict(config=config,source=source,groups=names,labels=labels,tests=int(suite.attrib['tests']),
                failures=int(suite.attrib['failures']),skips=int(suite.attrib['skipped']),exit_code=code)
    checks.append(result);(root/'checks.json').write_text(json.dumps(checks,indent=2)+'\n')
    assert code==result['failures']==result['skips']==0
print('PASS 207 runtime group executions; all three full 69-group suites; no skips',flush=True)
