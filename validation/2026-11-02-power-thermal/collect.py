"""Fixed read-only observation sequence. No device writes or elevated access."""
import datetime, hashlib, json, os, subprocess, time
from pathlib import Path

ROOT=Path('validation/2026-11-02-power-thermal')
def digest(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def utc(): return datetime.datetime.now(datetime.timezone.utc).isoformat()
def discover():
    fields={}
    def add(p, kind='int'): fields[str(p)]=kind
    def names(p, ints=(), texts=()):
        for n in ints: add(p/n)
        for n in texts: add(p/n,'text')
    for p in Path('/sys/devices/system/cpu/cpufreq').glob('policy*'):
        names(p,['scaling_min_freq','scaling_max_freq','scaling_cur_freq','cpuinfo_avg_freq','bios_limit'],
              ['scaling_driver','scaling_governor','related_cpus'])
    names(Path('/sys/devices/system/cpu/intel_pstate'),['no_turbo','min_perf_pct','max_perf_pct'],['status'])
    for p in Path('/sys/class/thermal').glob('thermal_zone*'):
        names(p,['temp'],['type','policy'])
        for f in p.glob('trip_point_*'): add(f,'text' if f.name.endswith('_type') else 'int')
    for p in Path('/sys/class/thermal').glob('cooling_device*'):
        names(p,['cur_state','max_state'],['type'])
    for p in Path('/sys/class/hwmon').glob('hwmon*'):
        try: coretemp=(p/'name').read_text().strip()=='coretemp'
        except OSError: coretemp=False
        if coretemp:
            add(p/'name','text')
            for f in p.glob('temp*'): add(f,'text' if f.name.endswith('_label') else 'int')
    for p in Path('/sys/devices/system/cpu').glob('cpu[0-9]*/thermal_throttle/*_throttle_count'): add(p)
    for p in Path('/sys/class/powercap').glob('intel-rapl*'):
        for n in ['name','enabled','energy_uj','max_energy_range_uj']:
            if (p/n).exists(): add(p/n,'text' if n=='name' else 'int')
        for f in p.glob('constraint_*'): add(f,'text' if f.name.endswith('_name') else 'int')
    for p in Path('/sys/class/power_supply').glob('*'):
        try: kind=(p/'type').read_text().strip()
        except OSError: continue
        if kind=='Mains': names(p,['online'],['type'])
    names(Path('/sys/class/power_supply/BAT0'),['capacity','voltage_now','current_now','power_now'],['status'])
    return dict(sorted(fields.items()))

def main():
    assert not (ROOT/'manifest.json').exists()
    assert not subprocess.check_output(['git','diff','--name-only','HEAD'],text=True).strip()
    binary=Path('build/release/feature_lab').resolve()
    fields=discover(); start=time.monotonic()
    manifest=dict(schema='power_thermal_observation_v1',source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                  started_utc=utc(),binary=str(binary),binary_sha256=digest(binary),
                  protocol_sha256=digest('benchmarks/POWER_THERMAL_OBSERVATION.md'),
                  collector_sha256=digest(__file__),interval_seconds=.5,fields=fields,phases=[])
    def save(): (ROOT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    def sample():
        began=time.monotonic();values={}
        for path,kind in fields.items():
            try:
                raw=Path(path).read_text().strip()
                value=int(raw) if kind=='int' else raw
                values[path]=dict(value=value,error=None)
            except (OSError,ValueError) as e:
                values[path]=dict(value=None,error=dict(type=type(e).__name__,errno=getattr(e,'errno',None)))
        # Host-wide observations; never interpreted as per-engine CPU demand.
        cpu=[int(x) for x in Path('/proc/stat').read_text().splitlines()[0].split()[1:]]
        return dict(offset_seconds=began-start,read_seconds=time.monotonic()-began,
                    values=values,host_cpu_ticks=cpu,load_average=list(os.getloadavg()))
    def phase(name, duration=None, cpu=False):
        print('START',name,flush=True)
        began=time.monotonic();cpu_start=time.process_time();samples=[sample()]
        record=dict(name=name,started_utc=utc(),start_offset_seconds=began-start,
                    observations=name+'-observations.json',command=None)
        if duration is not None:
            deadline=time.monotonic()+duration
            while time.monotonic()<deadline:
                time.sleep(min(.5,max(0,deadline-time.monotonic())))
                samples.append(sample())
            record.update(requested_seconds=duration,exit_code=None)
        else:
            report=ROOT/(name+'.json');log=ROOT/(name+'.log')
            assert not report.exists() and not log.exists()
            args=[str(binary),'--stress','--width','1920','--height','1080','--frames','600',
                  '--warmup','600','--cycles','3','--report',str(report)]
            args += ['--cpu-only'] if cpu else ['--frame-samples','--submission-samples',
                     '--preparation-samples','--context-samples','--presentation','immediate']
            record.update(command=args,report=report.name,log=log.name,launch_offset_seconds=time.monotonic()-start)
            with log.open('x') as output:
                child=subprocess.Popen(args,stdout=output,stderr=subprocess.STDOUT)
                timed_out=False
                while True:
                    pid,status,usage=os.wait4(child.pid,os.WNOHANG)
                    if pid: break
                    if time.monotonic()-began>180:
                        child.kill();_,status,usage=os.wait4(child.pid,0);timed_out=True;break
                    samples.append(sample());time.sleep(.5)
                child.returncode=os.waitstatus_to_exitcode(status)
            record.update(exit_code=child.returncode,timed_out=timed_out,rss_peak_bytes=usage.ru_maxrss*1024,
                          user_seconds=usage.ru_utime,system_seconds=usage.ru_stime,
                          report_sha256=digest(report) if report.exists() else None,log_sha256=digest(log))
            samples.append(sample())
        output=ROOT/record['observations'];assert not output.exists()
        output.write_text(json.dumps(samples,indent=2)+'\n')
        record.update(end_offset_seconds=time.monotonic()-start,finished_utc=utc(),
                      collector_cpu_seconds=time.process_time()-cpu_start,
                      observations_sha256=digest(output))
        manifest['phases'].append(record);save();print('DONE',name,record['exit_code'],flush=True)
    save();phase('before',duration=20);phase('cpu-reference',cpu=True)
    for i in range(1,7): phase('graphics-'+str(i))
    phase('after',duration=60)
    manifest['finished_utc']=utc();save()
if __name__=='__main__': main()
