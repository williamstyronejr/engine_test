"""Compare the extracted source to the retained fixed-commit archive byte for byte."""
import json
from pathlib import Path
import tarfile

root = Path('validation/2026-10-29-offline')
source = json.loads((root/'source.json').read_text())
count = 0
with tarfile.open(source['archive']) as archive:
    for member in archive.getmembers():
        if member.isfile():
            assert archive.extractfile(member).read() == (Path(source['source'])/member.name).read_bytes(), member.name
            count += 1
record = dict(files_checked=count, all_bytes_match=True, source_revision=source['revision'],
              source_archive_sha256=source['sha256'])
destination = root/'source-extraction-check.json'
if destination.exists():
    assert json.loads(destination.read_text()) == record
else:
    destination.write_text(json.dumps(record, indent=2)+'\n')
print(count, 'source files match archived bytes')
