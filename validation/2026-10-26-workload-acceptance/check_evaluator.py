"""Controlled evidence mutations; synthetic fixtures are never benchmark results."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile

source = Path('validation/2026-10-26-workload-acceptance/evaluate.py')
spec = importlib.util.spec_from_file_location('acceptance', source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
original = Path(sys.argv[1])
results = []
with tempfile.TemporaryDirectory(prefix='acceptance-evaluator-') as temp:
    root = Path(temp)
    for p in original.iterdir():
        if p.suffix in ('.json', '.log'):
            shutil.copyfile(p, root / p.name)
    base = module.read(root / 'manifest.json')
    docs = {}
    for entry in base['runs']:
        doc = module.read(root / entry['report'])
        for dist in doc['milliseconds'].values():
            for field in ('mean', 'median', 'p95', 'p99', 'max'):
                dist[field] = 1.0 if dist['samples'] else 0.0
        if 'spikes' in doc:
            for event in doc['spikes']['events']:
                event['operation_ms'] = 1.0
        entry['rss_peak_bytes'] = 1024 * 1024
        docs[entry['report']] = doc

    def check(name, mutate, expected, missing=None):
        manifest, reports = copy.deepcopy(base), copy.deepcopy(docs)
        mutate(manifest, reports)
        for filename, doc in reports.items():
            (root / filename).write_text(json.dumps(doc) + '\n')
        for entry in manifest['runs']:
            entry['report_sha256'] = module.digest(root / entry['report'])
        (root / 'manifest.json').write_text(json.dumps(manifest) + '\n')
        if missing:
            (root / missing).unlink()
        try:
            out = module.evaluate(root)
            actual = 'pass' if out['automated_budgets_passed'] else 'budget_failure'
        except (module.InvalidEvidence, KeyError, TypeError, ValueError, OSError):
            actual = 'invalid_evidence'
        if actual != expected:
            raise RuntimeError((name, actual, expected))
        results.append(dict(case=name, expected=expected, observed=actual, passed=True))

    check('synthetic_all_budgets_pass', lambda m,d: None, 'pass')
    check('missing_attempt', lambda m,d: m['runs'].pop(), 'invalid_evidence')
    check('duplicate_attempt', lambda m,d: m['runs'].__setitem__(1, copy.deepcopy(m['runs'][0])), 'invalid_evidence')
    check('missing_report', lambda m,d: None, 'invalid_evidence', 'combined-1.json')
    check('wrong_policy_hash', lambda m,d: m.__setitem__('policy_sha256','bad'), 'invalid_evidence')
    check('failed_process', lambda m,d: m['runs'][0].__setitem__('exit_code',1), 'invalid_evidence')
    check('stale_build', lambda m,d: d['combined-1.json']['build'].__setitem__('revision','old'), 'invalid_evidence')
    check('missing_samples', lambda m,d: d['combined-1.json']['milliseconds']['frame'].__setitem__('samples',0), 'invalid_evidence')
    check('nan_timing', lambda m,d: d['combined-1.json']['milliseconds']['frame'].__setitem__('max',float('nan')), 'invalid_evidence')
    check('changed_workload', lambda m,d: d['facility-1.json']['workload'].__setitem__('commands',2999), 'invalid_evidence')
    check('changed_gameplay', lambda m,d: d['annex-1.json']['run'].__setitem__('final_hash','0'), 'invalid_evidence')
    check('resized', lambda m,d: d['facility-1.json']['run'].__setitem__('resized',True), 'invalid_evidence')
    check('missing_gpu_support', lambda m,d: d['combined-1.json']['gpu_queries'].__setitem__('supported',False), 'invalid_evidence')
    check('skipped_gpu', lambda m,d: d['combined-1.json']['gpu_queries'].__setitem__('skipped',1), 'invalid_evidence')
    check('unverified_event', lambda m,d: d['spikes-1.json']['spikes']['events'][0].__setitem__('verified',False), 'invalid_evidence')
    check('retained_staging', lambda m,d: d['spikes-1.json']['spikes']['events'][0]['staging'].__setitem__('cpu_remaining_bytes',1), 'invalid_evidence')
    check('rss_over_budget', lambda m,d: m['runs'][0].__setitem__('rss_peak_bytes',268435457), 'budget_failure')
    check('max_over_budget', lambda m,d: d['combined-1.json']['milliseconds']['frame'].__setitem__('max',16.700001), 'budget_failure')
    def tail(m,d):
        d['combined-1.json']['milliseconds']['frame'].update(p99=16.700001,max=17)
    check('p99_over_budget', tail, 'budget_failure')
    check('operation_over_budget', lambda m,d: d['spikes-1.json']['spikes']['events'][0].__setitem__('operation_ms',16.700001), 'budget_failure')
    def staging(m,d):
        for filename, doc in d.items():
            if filename.startswith('spikes-'):
                doc['spikes']['events'][0]['staging']['cpu_peak_bytes']=163841
                doc['spikes']['events'][0]['staging']['combined_peak_bytes']=163841
    check('staging_over_budget', staging, 'budget_failure')
    def at_boundary(m,d):
        for entry in m['runs']:
            entry['rss_peak_bytes']=268435456
        for doc in d.values():
            doc['milliseconds']['frame'].update(p99=16.7,max=16.7)
    check('inclusive_budget_boundary', at_boundary, 'pass')
print(json.dumps(dict(synthetic_controls=True, cases=results),indent=2))
