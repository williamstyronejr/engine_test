"""Positive/negative controls for function sample attribution and evidence-quality gates."""
import json
from pathlib import Path
from profile_analysis import analyze
exe='/example/feature_lab'
valid='42/42 123 hot() ('+exe+')\n'
text=valid*600+'42/42 456 [unknown] (/example/driver.so)\n';stats=' SAMPLE events: 601\n'
result=analyze(text,stats,exe)
assert result['application_samples']==600 and result['unknown_all_samples']==1
assert result['functions'][0]['percent_application']==100
cases=[('mixed_tid',text.replace('42/42','42/43',1),stats),
 ('mixed_pid',text.replace('42/42','43/43',1),stats),('malformed','bad\n'+text,stats),
 ('missing_object',text.replace(' (/example/feature_lab)','',1),stats),
 ('count_mismatch',text,' SAMPLE events: 600\n'),('lost',text,stats+' LOST events: 1\n'),
 ('throttled',text,stats+' THROTTLE events: 1\n'),
 ('insufficient',valid*499,' SAMPLE events: 499\n'),
 ('unknown_application',text.replace('hot()','[unknown]'),stats),
 ('inconsistent_counts',text,stats+' SAMPLE events: 600\n')]
checks=[dict(control='valid_with_unknown_external',accepted=True)]
for name,t,s in cases:
 try:analyze(t,s,exe)
 except ValueError:checks.append(dict(control=name,accepted=False))
 else:raise AssertionError(name)
Path(__file__).with_name('profile-controls.json').write_text(json.dumps(checks,indent=2)+'\n')
print('Valid profile accepted; ten invalid profiles rejected')
