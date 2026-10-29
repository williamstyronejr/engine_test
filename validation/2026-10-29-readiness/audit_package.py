"""Read-only archive audit and bounded CPU sample commands in a private extraction.

Usage: python3 audit_package.py ARCHIVE OUTPUT_DIRECTORY
TMPDIR must name disk-backed scratch space outside the repository.
No interactive play, physical devices, external URLs or Markdown anchors are tested.
"""
import hashlib
import json
import os
from pathlib import Path
import posixpath
import re
import subprocess
import sys
import tarfile
import tempfile
import time
from urllib.parse import unquote, urlsplit

archive = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=False)
repo = Path.cwd()
work = Path(tempfile.mkdtemp(prefix='package-doc-audit-'))
environment = {k: v for k, v in os.environ.items() if k not in ['DISPLAY', 'LD_LIBRARY_PATH', 'LD_PRELOAD']}
commands = []


def run(name, args, cwd):
    started = time.monotonic()
    result = subprocess.run([str(x) for x in args], cwd=cwd, env=environment,
                            capture_output=True, text=True, timeout=60)
    (output / (name+'.log')).write_text(result.stdout+result.stderr)
    commands.append(dict(name=name, command=[str(x) for x in args], cwd=str(cwd),
                         unset_environment=['DISPLAY', 'LD_LIBRARY_PATH', 'LD_PRELOAD'],
                         exit_code=result.returncode, seconds=time.monotonic()-started))
    (output / 'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    return result.stdout


with archive.open('rb') as stream:
    checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
assert archive.with_suffix(archive.suffix+'.sha256').read_text().split()[0] == checksum
links, missing, recipes, recipe_failures = [], [], 0, []
with tarfile.open(archive) as tar:
    names = set(tar.getnames())
    for member in tar.getmembers():
        if not member.isfile() or not member.name.endswith('.md'):
            continue
        document = tar.extractfile(member).read().decode()
        for code in re.findall(r'```sh\n(.*?)```', document, re.S):
            result = subprocess.run(['bash', '-n'], input=code, capture_output=True, text=True)
            recipes += 1
            if result.returncode:
                recipe_failures.append(dict(file=member.name, code=code, error=result.stderr))
        prose = re.sub(r'```.*?```', '', document, flags=re.S)
        for link in re.findall(r'\]\(([^)]+)\)', prose):
            if urlsplit(link).scheme or link.startswith('#'):
                continue
            target = unquote(link.split('#')[0].strip('<>'))
            dest = posixpath.normpath(posixpath.join(posixpath.dirname(member.name), target))
            record = dict(file=member.name, target=target, resolved=dest)
            links.append(record)
            if dest not in names:
                missing.append(record)
    (output / 'documentation.json').write_text(json.dumps(dict(
        archive=str(archive), sha256=checksum, relative_links=len(links), missing=missing,
        shell_recipes=recipes, recipe_failures=recipe_failures,
        excluded_checks=['external URLs', 'Markdown anchors', 'interactive recipe execution']), indent=2)+'\n')
    assert not missing and not recipe_failures
    tar.extractall(work / 'unpacked', filter='data')
roots = list((work / 'unpacked').iterdir())
assert len(roots) == 1
installed = work / 'relocated package with spaces'
roots[0].rename(installed)
cwd = work / 'unrelated cwd'
cwd.mkdir()
for name in ['BUILD_INFO.txt', 'RUNTIME_DEPENDENCIES.txt']:
    (output / (name+'.log')).write_bytes((installed / name).read_bytes())
run('manifest', [repo / '.tools/bin/cmake', '-DMODE=verify', '-DROOT='+str(installed),
                 '-P', repo / 'cmake/PackageSmoke.cmake'], cwd)
game = installed / 'bin/feature_lab'
validator = installed / 'bin/validate_assets'
bench = installed / 'bin/stress_bench'
help_text = run('game-help', [game, '--help'], cwd)
assert '--verify' in help_text and '--user-data' in help_text and '--no-audio' in help_text
run('stress-help', [game, '--stress', '--help'], cwd)
run('validator-help', [validator, '--help'], cwd)
run('game-verify', [game, '--verify'], cwd)
assert '20 checks, 0 failed' in run('validate-all', [validator], cwd)
assert '1 check, 0 failed' in run('validate-scene', [validator, '--type', 'scene', '--file',
                                                 'facility.scene', '--scene-budget', '256'], cwd)
validation = json.loads(run('validate-json', [validator, '--json'], cwd))
assert validation['complete'] and validation['passed']
(output / 'asset-validation.json').write_text(json.dumps(validation, indent=2)+'\n')
report = work / 'stress.json'
run('stress-bench', [bench, '--frames', '60', '--warmup', '10', '--cycles', '2',
                     '--report', report], cwd)
stress = json.loads(report.read_text())
assert stress['complete'] and stress['resources_stable'] and stress['run']['measured_frames'] == 120
(output / 'stress.json').write_bytes(report.read_bytes())
run('manifest-after', [repo / '.tools/bin/cmake', '-DMODE=verify', '-DROOT='+str(installed),
                       '-P', repo / 'cmake/PackageSmoke.cmake'], cwd)
(output / 'summary.json').write_text(json.dumps(dict(
    archive=str(archive), sha256=checksum, extracted_to=str(installed),
    relative_links=len(links), shell_recipes=recipes, successful_commands=len(commands),
    package_payload_unchanged=True, physical_or_interactive_acceptance=False), indent=2)+'\n')
print('PASS', len(links), 'links;', recipes, 'shell recipes;', len(commands), 'CPU commands', flush=True)
