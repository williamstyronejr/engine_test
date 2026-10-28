"""Validate retained perf exports; report exclusive sample counts without dropping unknowns."""
import collections, re

def analyze(samples, stats, executable):
    counts=collections.Counter(); threads=set()
    for line in samples.splitlines():
        if not line.strip(): continue
        m=re.fullmatch(r'\s*(\d+)/(\d+)\s+([0-9a-fA-F]+)\s+(.+)',line)
        if not m: raise ValueError('Malformed sample line')
        pid,tid=int(m[1]),int(m[2]);threads.add((pid,tid))
        if pid!=tid: raise ValueError('Non-main-thread sample')
        symbol,sep,obj=m[4].rpartition(' (')
        if not sep or not obj.endswith(')') or not symbol:raise ValueError('Missing symbol/object')
        counts[(obj[:-1],symbol)]+=1
    if len(threads)!=1:raise ValueError('Expected one process/thread')
    records={}
    for name,n in re.findall(r'^\s*(\S+) events:\s*(\d+)',stats,re.M):
        n=int(n)
        if name in records and records[name]!=n:raise ValueError('Inconsistent event counts')
        records[name]=n
    total=sum(counts.values())
    if records.get('SAMPLE')!=total:raise ValueError('Sample count mismatch')
    if any(n for k,n in records.items() if 'LOST' in k or 'THROTTLE' in k):raise ValueError('Lost/throttled records')
    app=sum(n for (obj,_),n in counts.items() if obj==executable)
    unknown=lambda s:s=='[unknown]' or s.startswith('0x')
    missing=sum(n for (obj,s),n in counts.items() if obj==executable and unknown(s))
    if app<500 or missing/app>=0.05:raise ValueError('Insufficient application-symbol coverage')
    objects=collections.Counter()
    for (obj,_),n in counts.items():objects[obj]+=n
    return dict(total_samples=total,application_samples=app,unknown_application_samples=missing,
        unknown_all_samples=sum(n for (_,s),n in counts.items() if unknown(s)),pid_tid=list(next(iter(threads))),
        records=records,objects=[dict(object=o,samples=n) for o,n in objects.most_common()],
        functions=[dict(object=o,symbol=s,samples=n,percent_all=100*n/total,
                   percent_application=100*n/app if o==executable else None)
                   for (o,s),n in counts.most_common()],quality_passed=True)
