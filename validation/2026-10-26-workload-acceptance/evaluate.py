"""Fail-closed evaluator for this declaration; no runtime or test-suite dependency."""
import hashlib
import json
import math
from pathlib import Path
import sys


class InvalidEvidence(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise InvalidEvidence(message)


def read(path):
    def invalid(value):
        raise InvalidEvidence('Nonfinite JSON constant: ' + value)
    return json.loads(path.read_text(), parse_constant=invalid)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def finite(value):
    return type(value) in (int, float) and math.isfinite(value) and value >= 0


def evaluate(root):
    policy = read(root / 'policy.json')
    manifest = read(root / 'manifest.json')
    require(policy['schema'] == 'feature_lab_acceptance_policy_v1', 'Policy schema')
    require(manifest['schema'] == 'feature_lab_acceptance_collection_v1', 'Collection schema')
    require(manifest['policy_sha256'] == digest(root / 'policy.json'), 'Declaration hash')
    require('finished_utc' in manifest, 'Collection did not finish')
    expected = {(name, i) for name in policy['workloads'] for i in range(1, policy['repeats'] + 1)}
    require(len(manifest['runs']) == len(expected), 'Attempt count')
    seen, structural, machines = set(), {}, {}
    results = []
    for entry in manifest['runs']:
        name, repeat = entry['name'], entry['repeat']
        key = name, repeat
        require(key in expected and key not in seen, 'Missing/duplicate/unexpected attempt')
        seen.add(key)
        require(entry['exit_code'] == 0, f'{key}: process exit')
        require(entry['report'] == f'{name}-{repeat}.json' and entry['log'] == f'{name}-{repeat}.log', 'Attempt paths')
        require(digest(root / entry['report']) == entry['report_sha256'], 'Report hash')
        require(digest(root / entry['log']) == entry['log_sha256'], 'Log hash')
        d = read(root / entry['report'])
        w = policy['workloads'][name]
        require(entry['command'][:-1] == w['command'] + [w['report_flag']], 'Command differs from policy')
        require(Path(entry['command'][-1]).name == entry['report'], 'Command report path')
        require(d['workload'] == w['workload'], f'{key}: workload identity')
        require(d['build']['revision'] == manifest['source'][:7], 'Build/source identity')
        require(d['build']['configuration'] == 'Release' and 'sanitizers=OFF' in d['build']['flags'], 'Build configuration')
        require(d['build']['compiler'] == 'GNU 16.2.1', 'Declared compiler')
        cpu = name in ('dense', 'mixer')
        machine_key = 'cpu' if cpu else 'gpu'
        identity = (d['machine'], d['build'])
        require(machines.setdefault(machine_key, identity) == identity, 'Machine/build changed')
        run, times, q = d['run'], d['milliseconds'], d['gpu_queries']
        require(run['measured_frames'] == w['measured_frames'], 'Measured frame count')
        require(run['vsync_requested'] is False and run['audio_device'] is False, 'Pacing/audio configuration')
        scopes = ('load', 'simulation', 'mixer', 'render_submit', 'preview_submit', 'present', 'frame', 'gpu_render') if w['kind'] == 'stress' else ('simulation', 'minimap_submit', 'world_submit', 'ui_submit', 'present', 'frame', 'gpu_render')
        require(set(times) == set(scopes), 'Timing scope inventory')
        for scope, dist in times.items():
            require(type(dist['samples']) is int and dist['samples'] >= 0, 'Sample count')
            require(all(finite(dist[k]) for k in ('mean', 'median', 'p95', 'p99', 'max')), 'Timing number')
            require(dist['median'] <= dist['p95'] <= dist['p99'] <= dist['max'] and dist['mean'] <= dist['max'], 'Timing ordering')
            if scope == 'gpu_render':
                require(dist['samples'] == 0 if cpu else w['measured_frames'] - 8 <= dist['samples'] <= w['measured_frames'], 'GPU samples')
            else:
                n = 3 if scope == 'load' else 0 if cpu and scope in ('render_submit', 'preview_submit', 'present') else w['measured_frames']
                require(dist['samples'] == n, f'{scope}: samples')
        if cpu:
            require(q['supported'] is False and all(q[k] == 0 for k in ('submitted', 'completed', 'pending', 'skipped', 'invalid')), 'CPU query counts')
        else:
            require(q['supported'] is True and q['method'] == 'time_elapsed' and q['counter_bits'] > 0, 'GPU method')
            require(q['submitted'] == w['total_frames'] and q['completed'] + q['pending'] == q['submitted'], 'GPU accounting')
            require(0 <= q['pending'] <= 8 and q['skipped'] == q['invalid'] == 0 and q['failures'] == [] and q['failures_omitted'] == 0, 'GPU failures')
            require(run['vsync_disable_accepted'] is True, 'VSync-off request')
        checks = []

        def limit(label, value, maximum):
            require(finite(value), label + ': invalid number')
            checks.append(dict(name=label, observed=value, ceiling=maximum, passed=value <= maximum))

        require(finite(entry['rss_peak_bytes']) and entry['rss_peak_bytes'] > 0, 'Missing kernel peak RSS')
        limit('rss_peak_bytes', entry['rss_peak_bytes'], policy['rss_peak_bytes'])
        limit('frame_p99_ms', times['frame']['p99'], policy['frame_p99_ms'])
        limit('frame_max_ms', times['frame']['max'], policy['frame_max_ms'])
        if w['kind'] == 'stress':
            require(d['schema'] == 'feature_lab_stress_v1' and d['workload_version'] == 1, 'Stress schema')
            require(d['complete'] is True and d['resources_stable'] is True and run['completed_cycles'] == 3, 'Stress completion/resources')
            require(d['mode'] == ('cpu' if cpu else 'opengl'), 'Stress mode')
            require(run['capture_requested'] is False, 'Screenshot perturbs timing')
            mem = d['memory']
            limit('tracked_cpu_buffer_bytes', mem['tracked_cpu_buffer_bytes'], policy['stress_cpu_bytes'])
            limit('report_buffer_bytes', mem['report_buffer_bytes'], policy['stress_report_bytes'])
            for field, value in [('peak_uploaded_textures', 1), ('peak_uploaded_texture_bytes', 16), ('peak_targets', 1), ('peak_target_bytes', 8294400)]:
                require(mem[field] == (0 if cpu else value), field)
            signature = (d['workload'], run['checksum'], d['totals'], mem['tracked_cpu_buffer_bytes'], mem['report_buffer_bytes'], run['window_width'], run['window_height'])
            if not cpu:
                require(run['checksum'] == '3573445382734126645', 'Combined checksum')
                require(d['totals'] == dict(quads=18432000, culled=3686400, draws=5400, visible_tiles=7372800, collision_candidates=1396800, contacts=0), 'Combined counts')
        else:
            require(d['schema'] == ('feature_lab_spike_profile_v1' if w['kind'] == 'spikes' else 'feature_lab_replay_profile_v1'), 'Game schema')
            require(run['complete'] is True and run['resized'] is False and run['frames'] == w['total_frames'], 'Game completion/viewport')
            require(run['final_hash'] == w['final_hash'] and run['ticks'] == w['ticks'], 'Gameplay result')
            require(d['totals'] == w['totals'], 'Gameplay render/simulation counts')
            signature = (d['workload'], run['final_hash'], d['totals'], run['width'], run['height'])
            if w['kind'] == 'spikes':
                spikes = d['spikes']
                require(spikes['complete'] is True and spikes['resources_stable'] is True and spikes['expected_events'] == len(spikes['events']) == 10, 'Spike completion')
                for event, reference in zip(spikes['events'], w['events']):
                    require({k: event[k] for k in reference} == reference and event['verified'] is True, 'Event identity/state')
                    s = event['staging']
                    require(s['coverage'] == 'owned_payload_v2' and s['measured'] is True and s['cpu_remaining_bytes'] == s['gpu_remaining_bytes'] == 0, 'Staging cleanup/coverage')
                    for field in ('cpu', 'gpu', 'combined'):
                        limit(f"event_{event['frame']}_{field}_bytes", s[field + '_peak_bytes'], policy['staging_' + field + '_bytes'])
                    limit(f"event_{event['frame']}_operation_ms", event['operation_ms'], policy['operation_max_ms'])
                    for state in (event['before'], event['after']):
                        require(state['textures'] == 9 and state['targets'] == 1, 'Live graphics handles')
                        for field, maximum in [('texture_bytes', 32768), ('target_bytes', 131072), ('font_bytes', 16384)]:
                            limit(f"event_{event['frame']}_{field}", state[field], maximum)
                for category, coverage in [('scene', 'successful_mutations_v1'), ('collision', 'successful_snapshots_v1')]:
                    memory = spikes[category + '_memory']
                    require(memory['coverage'] == coverage and [x['level'] for x in memory['levels']] == [0, 1], 'Utilization coverage')
                    for level in memory['levels']:
                        limit(f"{category}_level_{level['level']}_bytes", level['reserved_bytes'], policy[category + '_bytes'])
                signature += (spikes['scene_memory'], spikes['collision_memory'], [e['staging'] for e in spikes['events']])
        require(structural.setdefault(name, signature) == signature, 'Structural repeat mismatch')
        results.append(dict(name=name, repeat=repeat, frame=times['frame'], checks=checks, passed=all(x['passed'] for x in checks)))
    require(seen == expected, 'Missing attempts')
    return dict(schema='feature_lab_acceptance_result_v1', source=manifest['source'], evidence_valid=True,
                automated_budgets_passed=all(r['passed'] for r in results), release_accepted=False, attempts=results)


def main():
    root = Path(sys.argv[1])
    try:
        result = evaluate(root)
    except (InvalidEvidence, KeyError, TypeError, ValueError, OSError) as exc:
        result = dict(schema='feature_lab_acceptance_result_v1', evidence_valid=False, error=str(exc), release_accepted=False)
    (root / 'evaluation.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'attempts'}, indent=2))
    return 2 if not result['evidence_valid'] else 0 if result['automated_budgets_passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
