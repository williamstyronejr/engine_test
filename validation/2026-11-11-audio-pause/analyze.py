"""Check retained transport evidence; audible artifacts remain user-reported failures."""
import datetime
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parent
session = root/'session'
prepared = json.loads((session/'prepared.json').read_text())
completion = json.loads((session/'completion.json').read_text())
assert completion['source'] == prepared['source']
assert completion['command'] == prepared['command']
assert completion['exit_code'] == 0
raw = (session/'game.log').read_text().splitlines()
timed = [line.split(' ', 1) for line in (session/'timed.log').read_text().splitlines()]
assert [line[1] for line in timed] == raw
times = [datetime.datetime.fromisoformat(line[0]) for line in timed]
assert times == sorted(times)

def fields(prefix):
    matches = [line for line in raw if line.startswith(prefix)]
    assert len(matches) == 1, (prefix, len(matches))
    return dict(re.findall(r'(\w+)=([^ ]+)', matches[0]))

audio = fields('[audio stats]')
state = fields('[session]')
assert audio['enabled'] == '1' and audio['device_error'] == '0'
assert audio['operation'] == 'none' and audio['pcm_state'] == '-1'
for key in ['dropped', 'queue_rejections', 'underruns', 'suspensions', 'recoveries']:
    assert audio[key] == '0'
assert int(audio['written_frames']) > 0
assert state['level'] == 'FACILITY' and state['level_changes'] == '1'
assert state['collected'] == '3' and state['door_completions'] == '1'
assert completion['first_failure'] is None
trace = fields('[alsa trace]')
assert trace['operation'] == 'open' and trace['result'] == '0'
assert not any('[audio] Muted:' in line for line in raw)
transitions = completion['transitions']
assert len(transitions) == 1 and transitions == json.loads((session/'transitions.json').read_text())
assert transitions[0]['line'].startswith('[level] entered=FACILITY ')
assert any(stamp == transitions[0]['received_utc'] and line == transitions[0]['line'] for stamp,line in timed)
duration = (completion['ended_monotonic_ns']-completion['started_monotonic_ns'])/1e9
residence = (completion['ended_monotonic_ns']-transitions[0]['monotonic_ns'])/1e9
assert duration == completion['elapsed_seconds']
assert residence == completion['first_facility_residence_seconds']
assert residence >= prepared['target_facility_seconds'] == 300
assert completion['target_residence_observed']
integrity = json.loads((session/'integrity.json').read_text())
assert integrity['archive_sha256'] == integrity['expected_sha256'] == prepared['archive_sha256']
assert integrity['manifest_exit_code'] == 0
for stage in ['before-launch', 'after-exit']:
    capture = json.loads((session/stage/'capture.json').read_text())
    assert all(item.get('exit_code') == 0 for item in capture['commands'])
    assert (session/stage/'command-0.log').read_text().strip() == '-- No entries --'
report = json.loads((root/'user-observations.json').read_text())
print(json.dumps(dict(source=prepared['source'], exit_code=0,
    elapsed_seconds=duration, facility_residence_seconds=residence,
    residence_target_met=True, audio=audio, session=state,
    original_EIO_reproduced=False, original_EIO_cause_resolved=False,
    audible_artifacts_reported=True, audible_artifacts_resolved=False,
    speaker_output_recorded=False, pickup_timestamps_available=False,
    user_observations=report), indent=2))
