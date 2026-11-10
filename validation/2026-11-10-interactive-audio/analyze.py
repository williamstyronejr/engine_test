"""Check retained session consistency; report observed scope, not release acceptance."""
import datetime
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parent
prepared = json.loads((root / 'prepared.json').read_text())
completion = json.loads((root / 'completion.json').read_text())
assert completion['source'] == prepared['source']
assert completion['command'] == prepared['command']
assert completion['status'] == 'exited' and completion['exit_code'] == 0
raw = (root / 'game.log').read_text().splitlines()
timed = [line.split(' ', 1) for line in (root / 'timed.log').read_text().splitlines()]
assert [line[1] for line in timed] == raw
times = [datetime.datetime.fromisoformat(line[0]) for line in timed]
assert times == sorted(times)

def one(prefix):
    matches = [(i, line) for i, line in enumerate(raw) if line.startswith(prefix)]
    assert len(matches) == 1, (prefix, len(matches))
    return matches[0]

def fields(line):
    return dict(re.findall(r'(\w+)=([^ ]+)', line))

_, audio_line = one('[audio stats]')
audio = fields(audio_line)
assert audio['enabled'] == '1' and audio['device_error'] == '0'
assert audio['operation'] == 'none' and audio['pcm_state'] == '-1'
for key in ['dropped', 'queue_rejections', 'underruns', 'suspensions', 'recoveries']:
    assert audio[key] == '0', (key, audio[key])
assert int(audio['written_frames']) > 0
session_index, session_line = one('[session]')
session = fields(session_line)
assert session['level'] == 'FACILITY' and session['level_changes'] == '1'
transition_index, _ = one('[level] entered=FACILITY')
traces = [fields(line) for line in raw if line.startswith('[alsa trace]')]
assert len(traces) == 1 and traces[0]['operation'] == 'open' and traces[0]['result'] == '0'
assert not any('[audio] Muted:' in line for line in raw)
assert json.loads((root / 'journal-command.json').read_text())['exit_code'] == 0
assert (root / 'audio-services.log').read_text().strip() == '-- No entries --'
assert json.loads((root / 'integrity.json').read_text())['package_payload_unchanged']
start = datetime.datetime.fromisoformat(completion['started_utc'])
end = datetime.datetime.fromisoformat(completion['ended_utc'])
assert start <= times[0] <= times[-1] <= end
print(json.dumps(dict(
    source=prepared['source'], exit_code=0, elapsed_seconds=(end-start).total_seconds(),
    rendered_frames=int(session['frames']), level_changes=1,
    final_level='FACILITY', final_level_ticks=int(session['ticks']),
    seconds_from_transition_log_to_final_session_log=(times[session_index]-times[transition_index]).total_seconds(),
    seconds_from_transition_log_to_exit=(end-times[transition_index]).total_seconds(),
    audio=audio, trace_operations=traces, audio_service_entries=0,
    user_reports=json.loads((root / 'user-observations.json').read_text()),
    original_failure_reproduced=False, original_cause_resolved=False,
    timing_or_endurance_qualification=False), indent=2))
