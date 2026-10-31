"""Validate observations without treating missing sensors as zeros or frame data."""
import math
from pathlib import PurePath


def number(value):
    assert type(value) in (int, float) and math.isfinite(value) and value >= 0
    return value


def validate(data, manifest, entry):
    assert data['schema'] == 'stress_host_observations_v1'
    static = manifest['static_paths']
    dynamic = manifest['dynamic_paths']
    assert len(set(static)) == len(static) and len(set(dynamic)) == len(dynamic)

    def values(items, paths, numeric=False):
        assert set(items) == set(paths)
        for path, item in items.items():
            assert set(item) == {'value', 'error'}
            value, error = item['value'], item['error']
            if value is None:
                assert isinstance(error, str) and error
            else:
                assert isinstance(value, str) and error is None
                if numeric:
                    assert str(int(value)) == value
                    if PurePath(path).name != 'temp':
                        assert int(value) >= 0

    values(data['static_before'], static)
    values(data['static_after'], static)
    launch = number(data['launch_offset_seconds'])
    exited = number(data['exit_observed_offset_seconds'])
    assert launch < exited <= number(entry['seconds'])
    cpu = number(data['collector_cpu_seconds'])
    assert cpu <= entry['seconds']
    samples = data['samples']
    assert len(samples) >= (3 if entry['telemetry'] else 2)
    assert samples[0]['label'] == 'before' and samples[-1]['label'] == 'after'
    assert all(s['label'] == 'running' for s in samples[1:-1])
    assert entry['telemetry'] or len(samples) == 2
    previous_end = 0
    for sample in samples:
        start = number(sample['offset_seconds'])
        end = start + number(sample['read_seconds'])
        assert previous_end <= start <= end <= entry['seconds']
        previous_end = end
        if sample['label'] == 'before':
            assert end <= launch
        elif sample['label'] == 'running':
            assert launch <= start < exited
        else:
            assert start >= exited
        values(sample['values'], dynamic, numeric=True)
    summary = {}
    for path in dynamic:
        valid = [int(s['values'][path]['value']) for s in samples if s['values'][path]['value'] is not None]
        item = dict(samples=len(valid), errors=len(samples)-len(valid),
                    minimum=min(valid) if valid else None, maximum=max(valid) if valid else None,
                    median=sorted(valid)[(len(valid)-1)//2] if valid else None)
        if path.endswith('_throttle_count'):
            before = samples[0]['values'][path]['value']
            after = samples[-1]['values'][path]['value']
            item['boundary_delta'] = int(after)-int(before) if before is not None and after is not None else None
            item['observed_regression'] = any(a > b for a, b in zip(valid, valid[1:]))
        summary[path] = item
    return dict(sample_count=len(samples), collector_cpu_seconds=cpu,
                total_read_seconds=sum(s['read_seconds'] for s in samples),
                max_read_seconds=max(s['read_seconds'] for s in samples),
                max_interval_seconds=max(b['offset_seconds']-a['offset_seconds'] for a, b in zip(samples, samples[1:])),
                static_changed=data['static_before'] != data['static_after'], paths=summary)
