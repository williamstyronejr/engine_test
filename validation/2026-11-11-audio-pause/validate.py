import datetime,json,os,subprocess,time
from pathlib import Path
work=Path(__file__).resolve().parent
root=Path('validation/2026-11-11-audio-pause')
scratch=Path((work/'scratch.txt').read_text().strip())
os.environ['PATH']=str(Path.cwd()/'.tools/bin')+':'+os.environ['PATH']
os.environ['TMPDIR']=str(scratch/'compiler-tmp')
os.environ['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1'
os.environ['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
records=[]
def run(name,command):
    print('START',name,flush=True)
    start=time.monotonic()
    stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
    with (root/(name+'.log')).open('x') as log:
        p=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    records.append(dict(name=name,command=command,source=source,started_utc=stamp,
        seconds=time.monotonic()-start,exit_code=p.returncode,
        environment={key:os.environ.get(key) for key in ['PATH','TMPDIR','ASAN_OPTIONS','UBSAN_OPTIONS','DISPLAY']}))
    (root/'validation-commands.json').write_text(json.dumps(records,indent=2)+'\n')
    print('DONE',name,p.returncode,flush=True)
    if p.returncode:
        print((root/(name+'.log')).read_text()[-6000:]);raise SystemExit(p.returncode)
common=['sound_tests','mixer_reload_tests','audio_tests','audio_reload_tests']
for name,kind,extra,targets,pattern in [
    ('debug','Debug',[],common+['audio_watch_tests'],r'^(sound|mixer_reload|audio_transport|audio_reload|audio_watch)$'),
    ('release','Release',[],common+['audio_watch_tests','audio_allocations','audio_ui_session_tests','feature_lab','stress_bench','validate_assets'],r'^(sound|mixer_reload|audio_transport|audio_reload|audio_watch|audio_allocations|audio_ui_session)$'),
    ('clang','Debug',['-DCMAKE_CXX_COMPILER=clang++'],common,r'^(sound|mixer_reload|audio_transport|audio_reload)$'),
    ('sanitize-platform','Debug',['-DENGINE_SANITIZERS=ON','-DENGINE_BUILD_PLATFORM=ON'],common,r'^(sound|mixer_reload|audio_transport|audio_reload)$')]:
    run(name+'-configure',['cmake','-S','.','-B','build/'+name,'-G','Ninja','-DCMAKE_BUILD_TYPE='+kind,
        '-DENGINE_PACKAGE_TEST_ROOT='+str(scratch/'packages'),*extra])
    run(name+'-build',['cmake','--build','build/'+name,'--target',*targets,'-j2'])
    run(name+'-tests',['ctest','--test-dir','build/'+name,'-R',pattern,'--output-on-failure',
        '--output-junit',str((root/(name+'-tests.xml')).resolve())])
