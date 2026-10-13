#!/usr/bin/env python3
"""Package a built native app with notices, screenshots, metadata and checksums."""
from pathlib import Path
import argparse
import hashlib
import json
import platform
import re
import shutil
import subprocess
import tempfile
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', type=Path, required=True)
parser.add_argument('--output-dir', type=Path, required=True)
parser.add_argument('--platform', required=True)
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
repository = project.parent
version = re.search(r'project\(LSFactory VERSION ([\d.]+)', (project / 'CMakeLists.txt').read_text()).group(1)
build = args.build_dir.resolve()
output = args.output_dir.resolve()
output.mkdir(parents=True, exist_ok=True)
product = build / 'LSFactory_artefacts' / 'Release'
app = product / ('LS Factory.app' if platform.system() == 'Darwin' else 'LS Factory.exe' if platform.system() == 'Windows' else 'LS Factory')
if not app.exists():
    raise SystemExit(f'Built app is missing: {app}')
name = f'LSFactory-{version}-{args.platform}'
with tempfile.TemporaryDirectory() as temporary:
    stage = Path(temporary) / name
    stage.mkdir()
    if app.is_dir():
        shutil.copytree(app, stage / app.name, symlinks=True)
    else:
        shutil.copy2(app, stage / app.name)
    # vcpkg links zlib dynamically by default on Windows.
    for dll in product.glob('*.dll'):
        shutil.copy2(dll, stage / dll.name)
    for path in [project / 'README.md', project / 'LICENSE']:
        shutil.copy2(path, stage / path.name)
    shutil.copytree(project / 'docs', stage / 'docs')
    shutil.copy2(project / 'resources/factory/LICENSE.txt', stage / 'FACTORY-CONTENT-LICENSE.txt')
    shutil.copytree(repository / 'screenshots', stage / 'screenshots')
    juce_candidates = [build / '_deps/juce-src', project / 'libs/JUCE', Path('/workspace/.tools/JUCE')]
    for juce in juce_candidates:
        notice = juce / 'LICENSE.md'
        if notice.exists():
            shutil.copy2(notice, stage / 'JUCE-LICENSE.md')
            break
    else:
        raise SystemExit('JUCE distribution notice was not found')
    sha = subprocess.check_output(['git', '-C', str(repository), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = bool(subprocess.check_output(['git', '-C', str(repository), 'status', '--porcelain'], text=True).strip())
    (stage / 'build-info.json').write_text(json.dumps({'version': version, 'commit': sha, 'dirty': dirty, 'platform': args.platform, 'architecture': platform.machine(), 'prerelease': True}, indent=2)+'\n')
    archive = output / f'{name}.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as zipped:
        for file in sorted(stage.rglob('*')):
            if file.is_file():
                zipped.write(file, file.relative_to(stage.parent))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (output / f'{archive.name}.sha256').write_text(f'{digest}  {archive.name}\n')
    print(archive)
