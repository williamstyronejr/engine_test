"""Collect the declared sequential workload. Linux/Python stdlib; run from repo root."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def temperatures():
    result = {}
    for path in Path('/sys/class/thermal').glob('thermal_zone*/temp'):
        try:
            result[str(path)] = int(path.read_text())
        except (OSError, ValueError):
            pass
    return result


def main():
    out = Path(sys.argv[1]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if any(out.iterdir()):
        raise RuntimeError('Output directory must be empty; retain previous attempts')
    if subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], text=True).strip():
        raise RuntimeError('Commit tracked changes before collecting')
    policy_path = HERE / 'policy.json'
    policy = json.loads(policy_path.read_text())
    (out / 'policy.json').write_bytes(policy_path.read_bytes())
    source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    inputs = sorted(Path('assets').rglob('*')) + sorted(Path('benchmarks/replays').glob('*.erpl'))
    manifest = dict(schema='feature_lab_acceptance_collection_v1', source=source,
                    started_utc=utc(), policy_sha256=digest(policy_path),
                    environment={k: os.environ.get(k) for k in ['DISPLAY', 'XDG_SESSION_TYPE', 'XDG_CURRENT_DESKTOP']},
                    binaries={str(p): digest(p) for p in ['build/release/feature_lab', 'build/release/stress_bench']},
                    inputs={str(p): digest(p) for p in inputs if p.is_file()}, runs=[])
    # wait4 collects this child's kernel lifetime peak, unlike cumulative RUSAGE_CHILDREN.
    for repeat in range(1, policy['repeats'] + 1):
        for name, workload in policy['workloads'].items():
            stem = f'{name}-{repeat}'
            args = workload['command'] + [workload['report_flag'], str(out / (stem + '.json'))]
            entry = dict(name=name, repeat=repeat, command=args, started_utc=utc(),
                         temperatures_before=temperatures(), report=stem + '.json', log=stem + '.log')
            print('START', stem, flush=True)
            start = time.monotonic()
            with (out / entry['log']).open('x') as log:
                child = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
                _, status, usage = os.wait4(child.pid, 0)
                child.returncode = os.waitstatus_to_exitcode(status)
            entry.update(exit_code=child.returncode, wall_seconds=time.monotonic() - start,
                         finished_utc=utc(), rss_peak_bytes=usage.ru_maxrss * 1024,
                         user_seconds=usage.ru_utime, system_seconds=usage.ru_stime,
                         temperatures_after=temperatures())
            report = out / entry['report']
            entry['report_sha256'] = digest(report) if report.is_file() else None
            entry['log_sha256'] = digest(out / entry['log'])
            manifest['runs'].append(entry)
            (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
            print('DONE', stem, 'exit', child.returncode, 'peak RSS', entry['rss_peak_bytes'], flush=True)
    manifest['finished_utc'] = utc()
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
