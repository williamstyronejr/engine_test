import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

run = Path(__file__).resolve().parent/'run-1'
meta = json.loads((run/'prepared.json').read_text())
utc = lambda: datetime.datetime.now(datetime.timezone.utc)
start = utc()

def write(name, value):
    temporary = run/(name+'.tmp')
    temporary.write_text(json.dumps(value, indent=2)+'\n')
    temporary.replace(run/name)

def capture(stage):
    folder = run/stage
    folder.mkdir()
    until = utc()
    commands = [
        ['journalctl', '--user', '-u', 'pipewire.service', '-u', 'pipewire-pulse.service',
         '-u', 'wireplumber.service', '--since', start.strftime('%Y-%m-%d %H:%M:%S UTC'),
         '--until', until.strftime('%Y-%m-%d %H:%M:%S UTC'), '--no-pager', '-o', 'short-iso'],
        ['systemctl', '--user', 'show', 'pipewire.service', 'pipewire-pulse.service',
         'wireplumber.service', '-p', 'Id', '-p', 'ActiveState', '-p', 'SubState',
         '-p', 'MainPID', '-p', 'ExecMainStatus'],
    ]
    records = []
    for i, command in enumerate(commands):
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=10)
            (folder/f'command-{i}.log').write_text(result.stdout+result.stderr)
            records.append(dict(command=command, exit_code=result.returncode))
        except (subprocess.TimeoutExpired, OSError) as error:
            records.append(dict(command=command, collection_error=str(error)))
    devices = {}
    for path in [Path('/proc/asound/cards'), Path('/proc/asound/pcm'),
                 *sorted(Path('/proc/asound').glob('card*/pcm*/sub*/status'))]:
        try:
            devices[str(path)] = path.read_text()
        except OSError as error:
            devices[str(path)] = str(error)
    (folder/'capture.json').write_text(json.dumps(dict(received_utc=until.isoformat(),
        commands=records, devices=devices), indent=2)+'\n')

capture('before-launch')
env = dict(os.environ, LD_PRELOAD=meta['LD_PRELOAD'])
started = utc()
started_mono = time.monotonic_ns()
p = subprocess.Popen(meta['command'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                     env=env, text=True, bufsize=1)
record = dict(pid=p.pid, command=meta['command'], source=meta['source'], status='running',
              started_utc=started.isoformat(), started_monotonic_ns=started_mono)
write('running.json', record)
first_failure = None
transitions = []
with (run/'game.log').open('x') as raw, (run/'timed.log').open('x') as timed:
    for line in p.stdout:
        received = utc().isoformat()
        mono = time.monotonic_ns()
        raw.write(line); raw.flush()
        timed.write(received+' '+line); timed.flush()
        event = dict(received_utc=received, monotonic_ns=mono, line=line.rstrip('\n'))
        if line.startswith('[level] entered='):
            transitions.append(event)
            write('transitions.json', transitions)
        trace_error = (line.startswith('[alsa trace]') and 'result=-' in line)
        if first_failure is None and ('[audio] Muted:' in line or trace_error):
            first_failure = event
            write('first-failure.json', event)
            capture('first-failure-context')
code = p.wait()
ended_mono = time.monotonic_ns()
record.update(status='exited', exit_code=code, ended_utc=utc().isoformat(),
              ended_monotonic_ns=ended_mono, elapsed_seconds=(ended_mono-started_mono)/1e9,
              transitions=transitions, first_failure=first_failure)
facility = next((event for event in transitions if 'entered=FACILITY ' in event['line']), None)
if facility:
    following = next((event for event in transitions if event['monotonic_ns'] > facility['monotonic_ns']), None)
    end = following['monotonic_ns'] if following else ended_mono
    record['first_facility_residence_seconds'] = (end-facility['monotonic_ns'])/1e9
    record['target_residence_observed'] = record['first_facility_residence_seconds'] >= meta['target_facility_seconds']
else:
    record['target_residence_observed'] = False
write('completion.json', record)
capture('after-exit')
archive = Path(meta['archive'])
manifest = subprocess.run(['sha256sum', '--quiet', '-c', 'FILES.sha256'],
    cwd=meta['extracted_to'], capture_output=True, text=True)
(run/'manifest-after.log').write_text(manifest.stdout+manifest.stderr)
write('integrity.json', dict(archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
    expected_sha256=meta['archive_sha256'], manifest_exit_code=manifest.returncode))
write('collection-complete.json', dict(completed_utc=utc().isoformat(), exit_code=code))
