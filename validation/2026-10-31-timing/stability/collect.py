"""Predeclared, read-only warmup/order study. Never overwrite or retry attempts."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def read_values(paths):
    result = {}
    for path in paths:
        try:
            result[str(path)] = {'value': path.read_text().strip(), 'error': None}
        except OSError as error:
            result[str(path)] = {'value': None, 'error': str(error)}
    return result


def main():
    binary = str(Path(sys.argv[1]).resolve())
    root = Path(sys.argv[2]) if len(sys.argv) > 2 else Path('validation/2026-10-28-stability')
    assert not subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], text=True).strip()
    assert digest(root / 'collect.py') == digest(__file__)
    assert not (root / 'manifest.json').exists()
    policies = sorted(Path('/sys/devices/system/cpu/cpufreq').glob('policy*'))
    zones = sorted(Path('/sys/class/thermal').glob('thermal_zone*'))
    throttles = sorted(Path('/sys/devices/system/cpu').glob('cpu[0-9]*/thermal_throttle/*_throttle_count'))
    static = [p / name for p in policies for name in (
        'scaling_driver', 'scaling_governor', 'scaling_min_freq', 'scaling_max_freq',
        'cpuinfo_min_freq', 'cpuinfo_max_freq', 'related_cpus', 'affected_cpus')]
    static += [p / 'type' for p in zones]
    dynamic = [p / name for p in policies for name in ('scaling_cur_freq', 'cpuinfo_avg_freq')]
    dynamic += [p / 'temp' for p in zones] + throttles
    manifest = dict(schema='stress_run_stability_collection_v1',
                    source=subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
                    started_utc=utc(), executable=binary, executable_sha256=digest(binary),
                    declaration_sha256=digest('benchmarks/STRESS_RUN_STABILITY.md'),
                    collector_sha256=digest(__file__), display=os.environ.get('DISPLAY'),
                    kernel=list(os.uname()), affinity=sorted(os.sched_getaffinity(0)),
                    priority=os.getpriority(os.PRIO_PROCESS, 0),
                    static_paths=list(map(str, static)), dynamic_paths=list(map(str, dynamic)),
                    telemetry_interval_seconds=0.25, references=[], runs=[])

    def save():
        (root / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

    def run(stem, warmup, telemetry, cpu_only=False, **metadata):
        report = root / (stem + '.json')
        log = root / (stem + '.log')
        sensors = root / (stem + '-sensors.json')
        assert not any(p.exists() for p in (report, log, sensors))
        args = [binary, '--stress', '--width', '1920', '--height', '1080',
                '--frames', '600', '--warmup', str(warmup), '--cycles', '3', '--report', str(report)]
        args += ['--cpu-only'] if cpu_only else ['--frame-samples', '--submission-samples',
                 '--preparation-samples', '--context-samples', '--presentation', 'immediate']
        entry = dict(stem=stem, warmup=warmup, telemetry=telemetry, command=args,
                     report=report.name, log=log.name, sensors=sensors.name, started_utc=utc(), **metadata)
        print('START', stem, flush=True)
        start = time.monotonic()
        cpu_start = time.process_time()
        samples = []

        def sample(label):
            began = time.monotonic()
            values = read_values(dynamic)
            samples.append(dict(label=label, offset_seconds=began-start,
                                read_seconds=time.monotonic()-began, values=values))

        before = read_values(static)
        sample('before')
        launch_offset = time.monotonic()-start
        with log.open('x') as output:
            process = subprocess.Popen(args, stdout=output, stderr=subprocess.STDOUT)
            if telemetry:
                while True:
                    pid, status, usage = os.wait4(process.pid, os.WNOHANG)
                    if pid:
                        break
                    sample('running')
                    time.sleep(0.25)
            else:
                _, status, usage = os.wait4(process.pid, 0)
            process.returncode = os.waitstatus_to_exitcode(status)
        exit_observed_offset = time.monotonic()-start
        sample('after')
        after = read_values(static)
        sensor_data = dict(schema='stress_host_observations_v1', static_before=before,
                           static_after=after, launch_offset_seconds=launch_offset,
                           exit_observed_offset_seconds=exit_observed_offset, samples=samples,
                           collector_cpu_seconds=time.process_time()-cpu_start)
        sensors.write_text(json.dumps(sensor_data, indent=2)+'\n')
        entry.update(exit_code=process.returncode, seconds=time.monotonic()-start,
                     rss_peak_bytes=usage.ru_maxrss*1024, finished_utc=utc(),
                     report_sha256=digest(report) if report.exists() else None,
                     log_sha256=digest(log), sensors_sha256=digest(sensors))
        manifest['references' if cpu_only else 'runs'].append(entry)
        save()
        print('DONE', stem, process.returncode, flush=True)

    save()
    for warmup in (60, 600):
        run(f'cpu-{warmup}', warmup, False, cpu_only=True)
    for repeat in range(1, 7):
        for position, warmup in enumerate((60, 600) if repeat % 2 else (600, 60), 1):
            run(f'w{warmup}-{repeat}', warmup, True, mode='short' if warmup == 60 else 'long',
                repeat=repeat, position=position, study='warmup')
    for repeat, enabled_order in ((1, (False, True)), (2, (True, False))):
        for position, enabled in enumerate(enabled_order, 1):
            mode = 'on' if enabled else 'off'
            run(f'observer-{mode}-{repeat}', 60, enabled, mode=mode,
                repeat=repeat, position=position, study='observer')
    manifest['finished_utc'] = utc()
    save()


if __name__ == '__main__':
    main()
