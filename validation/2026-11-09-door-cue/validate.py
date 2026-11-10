import datetime,json,os,subprocess,time
from pathlib import Path
w=Path('build/door-cue-refresh');scratch=Path((w/'scratch.txt').read_text().strip());os.environ['PATH']=str(Path.cwd()/'.tools/bin')+':'+os.environ['PATH'];os.environ['TMPDIR']=str(scratch/'compiler-tmp')
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip();commands=[]
for name,args in [('configure',['cmake','--preset','release','-DENGINE_PACKAGE_TEST_ROOT='+str(scratch/'packages')]),('build',['cmake','--build','--preset','release','--target','feature_lab','make_assets','sound_tests','audio_tests','audio_reload_tests','audio_watch_tests','audio_ui_session_tests','asset_validation_tests','validate_assets','-j2']),('tests',['ctest','--test-dir','build/release','--parallel','1','-R','^(sound|audio_transport|audio_reload|audio_watch|audio_ui_session|asset_validation|asset_validation_cli)$','--output-on-failure','--output-junit',str((w/'focused-tests.xml').resolve())])]:
 print('START',name,flush=True);start=time.monotonic()
 with (w/(name+'.log')).open('x') as log:p=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT)
 commands.append(dict(name=name,command=args,source=source,exit_code=p.returncode,seconds=time.monotonic()-start,environment={'TMPDIR':os.environ['TMPDIR'],'DISPLAY':os.environ.get('DISPLAY')}));(w/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 print('DONE',name,p.returncode,flush=True)
 if p.returncode:print((w/(name+'.log')).read_text()[-4000:]);raise SystemExit(p.returncode)
 if name=='tests':(w/'focused-full-output.log').write_bytes(Path('build/release/Testing/Temporary/LastTest.log').read_bytes())
