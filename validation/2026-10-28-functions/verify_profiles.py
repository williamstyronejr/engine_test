"""Recompute retained profile summaries and validate raw payload integrity without perf."""
import base64,hashlib,json,zlib
from pathlib import Path
from profile_analysis import analyze
root=Path(__file__).resolve().parent
profiles=json.loads((root/'profiles.json').read_text())
exe='/home/zero/dev/engine_test/build/release/feature_lab'
for p in profiles:
 stem=f"sample-{p['repeat']}"
 raw=json.loads((root/(stem+'-raw.json')).read_text())
 assert raw['schema']=='raw_perf_zlib_base64_v1'
 data=zlib.decompress(base64.b64decode(raw['data'],validate=True))
 assert len(data)==raw['bytes'] and hashlib.sha256(data).hexdigest()==raw['sha256']==p['raw_sha256']
 rebuilt=analyze((root/(stem+'-samples.log')).read_text(),(root/(stem+'-records.log')).read_text(),exe)
 assert rebuilt=={k:v for k,v in p.items() if k not in ('repeat','raw_sha256')}
print('Three raw payload hashes and all reconstructed profile summaries verified')
