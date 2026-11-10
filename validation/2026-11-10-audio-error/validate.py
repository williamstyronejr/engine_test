"""Focused diagnostics matrix; build sequentially and retain each first attempt."""
import datetime,json,os,subprocess,time,xml.etree.ElementTree as ET
from pathlib import Path
root=Path('validation/2026-11-10-audio-error');work=Path('build/audio-error-refresh');scratch=Path((work/'scratch.txt').read_text().strip())
os.environ['PATH']=str(Path.cwd()/'.tools/bin')+':'+os.environ['PATH'];os.environ['TMPDIR']=str(scratch/'compiler-tmp')
assert not (root/'validation-commands.json').exists()
assert not subprocess.check_output(['git','diff','--name-only','HEAD'],text=True).strip()
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip();commands=[];checks=[]
def run(name,args,env):
 print('START',name,flush=True);start=time.monotonic();stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
 with (root/(name+'.log')).open('x') as log:p=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,env=env)
 commands.append(dict(name=name,command=args,source=source,started_utc=stamp,seconds=time.monotonic()-start,exit_code=p.returncode,environment={k:env.get(k) for k in ['TMPDIR','DISPLAY','ASAN_OPTIONS','UBSAN_OPTIONS','TSAN_OPTIONS']}));(root/'validation-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 print('DONE',name,p.returncode,flush=True)
 return p.returncode
broad=['audio_recovery','audio_transport','audio_reload','audio_watch','audio_allocations','audio_ui_session']
small=['audio_recovery','audio_transport','audio_reload']
for config in ['debug','release','clang','sanitize-platform','readiness-tsan']:
 env=dict(os.environ);build=Path('build')/config
 args=['cmake','-S','.','-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE='+('Release' if config=='release' else 'Debug'),'-DENGINE_PACKAGE_TEST_ROOT='+str(scratch/'packages')]
 groups=broad if config in ['debug','release'] else small
 if config=='clang':args+=['-DCMAKE_CXX_COMPILER=clang++']
 if config=='sanitize-platform':
  args+=['-DENGINE_SANITIZERS=ON','-DENGINE_BUILD_PLATFORM=ON'];env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
 if config=='readiness-tsan':
  args+=['-DCMAKE_CXX_COMPILER=clang++','-DENGINE_SANITIZERS=OFF','-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer','-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread'];env['TSAN_OPTIONS']='halt_on_error=1';groups=['audio_transport','audio_reload']
 assert run(config+'-configure',args,env)==0
 mapping={'audio_transport':'audio_tests','audio_allocations':'audio_allocations'}
 targets=[mapping.get(g,g+'_tests') for g in groups]
 if config=='release':targets+=['feature_lab','audio_probe']
 assert run(config+'-build',['cmake','--build',str(build),'--target',*targets,'-j2'],env)==0
 xml=root/(config+'-tests.xml');code=run(config+'-tests',['ctest','--test-dir',str(build),'--parallel','1','-R','^('+'|'.join(groups)+')$','--output-on-failure','--output-junit',str(xml.resolve())],env)
 (root/(config+'-full-output.log')).write_bytes((build/'Testing/Temporary/LastTest.log').read_bytes())
 suite=ET.parse(xml).getroot();names=sorted(t.attrib['name'] for t in suite.findall('testcase'))
 result=dict(config=config,source=source,tests=int(suite.attrib['tests']),failures=int(suite.attrib['failures']),skips=int(suite.attrib['skipped']),groups=names,exit_code=code);checks.append(result);(root/'checks.json').write_text(json.dumps(checks,indent=2)+'\n')
 assert names==sorted(groups) and result['tests']==len(groups) and code==result['failures']==result['skips']==0
print('PASS 20 focused group executions; no skips',flush=True)
