import hashlib,json,subprocess,urllib.request
from pathlib import Path
root=Path('build/function-validation/perf');root.mkdir()
urls=subprocess.check_output(['pacman','-Sp','--print-format','%l','perf'],text=True).splitlines();records=[]
for url in urls:
 name=url.rsplit('/',1)[-1];dest=root/name
 urllib.request.urlretrieve(url,dest)
 stem=name.removesuffix('-x86_64.pkg.tar.zst').replace(':','-')
 # Repository entries use package-version without epoch in the directory.
 entries=subprocess.check_output(['bsdtar','-tf','/var/lib/pacman/sync/extra.db'],text=True).splitlines()
 package=name.split('-')[0]
 entry=next(e for e in entries if e.startswith(package+'-') and e.endswith('/desc'))
 desc=subprocess.check_output(['bsdtar','-xOf','/var/lib/pacman/sync/extra.db',entry],text=True)
 expected=desc.split('%SHA256SUM%\n')[1].splitlines()[0];actual=hashlib.sha256(dest.read_bytes()).hexdigest();assert expected==actual
 subprocess.run(['bsdtar','-xf',str(dest),'-C',str(root)],check=True)
 records.append(dict(url=url,sha256=actual,repository_sha256_matches=True,bytes=dest.stat().st_size))
Path('validation/2026-10-28-functions/local-profiler.json').write_text(json.dumps(records,indent=2)+'\n')
print('Verified/extracted',len(records),'packages under',root)
