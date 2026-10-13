#!/usr/bin/env python3
"""Build a genuine installer AppImage containing the verified workstation payload."""
from pathlib import Path
import argparse,hashlib,os,re,shutil,subprocess,tempfile,urllib.request
parser=argparse.ArgumentParser()
parser.add_argument('--build-dir',type=Path,required=True)
parser.add_argument('--output-dir',type=Path,required=True)
parser.add_argument('--tool-cache',type=Path,required=True)
args=parser.parse_args()
project=Path(__file__).resolve().parents[1]
version=re.search(r'project\(LSFactory VERSION ([\d.]+)',(project/'CMakeLists.txt').read_text()).group(1)
build=args.build_dir.resolve();output=args.output_dir.resolve();cache=args.tool_cache.resolve()
output.mkdir(parents=True,exist_ok=True);cache.mkdir(parents=True,exist_ok=True)
tool=cache/'linuxdeploy-x86_64.AppImage'
expected='c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d'
url='https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage'
if not tool.exists():
    with urllib.request.urlopen(url,timeout=60) as response,tool.open('wb') as destination:shutil.copyfileobj(response,destination)
if hashlib.sha256(tool.read_bytes()).hexdigest()!=expected:raise SystemExit('linuxdeploy SHA-256 verification failed')
tool.chmod(0o755)
extracted=cache/'squashfs-root'
if not (extracted/'AppRun').exists():subprocess.run([str(tool),'--appimage-extract'],cwd=cache,check=True,stdout=subprocess.DEVNULL)
launcher=extracted/'AppRun'
environment=os.environ.copy();environment['APPIMAGE_EXTRACT_AND_RUN']='1';environment['ARCH']='x86_64';environment['NO_STRIP']='1'
with tempfile.TemporaryDirectory(prefix='lsfactory-appimage-') as directory:
    temporary=Path(directory)
    def appimage(target,desktop,name):
        appdir=temporary/name
        binary=appdir/'usr/bin'/name;binary.parent.mkdir(parents=True)
        shutil.copy2(target,binary);binary.chmod(0o755)
        result=temporary/(name+'.AppImage')
        environment['OUTPUT']=str(result)
        subprocess.run([str(launcher),'--appdir',str(appdir),'--executable',str(binary),'--desktop-file',str(desktop),'--icon-file',str(project/'resources/icons/lsfactory.svg'),'--output','appimage'],cwd=temporary,env=environment,check=True)
        if not result.exists():raise RuntimeError('AppImage output was not created')
        return appdir,result
    notices=temporary/'lsfactory/usr/share/lsfactory'
    notices.mkdir(parents=True)
    shutil.copy2(project/'LICENSE',notices/'LICENSE')
    shutil.copytree(project/'docs',notices/'docs')
    for source in [build/'_deps/juce-src/LICENSE.md',project/'libs/JUCE/LICENSE.md',Path('/workspace/.tools/JUCE/LICENSE.md')]:
        if source.exists():shutil.copy2(source,notices/'JUCE-LICENSE.md');break
    else:raise RuntimeError('JUCE licence notice missing')
    appdir,program=appimage(build/'LSFactory_artefacts/Release/LS Factory',project/'packaging/linux/app.desktop','lsfactory')
    setupdir=temporary/'lsfactory-installer'
    payload=setupdir/'usr/share/lsfactory';payload.mkdir(parents=True)
    shutil.copy2(program,payload/'LSFactory.AppImage')
    (payload/'payload.sha256').write_text(hashlib.sha256(program.read_bytes()).hexdigest()+'\n')
    shutil.copy2(project/'resources/icons/lsfactory.svg',payload/'lsfactory.svg')
    # Use the same Installation implementation before creating the final image.
    test=temporary/'installer-test'
    subprocess.run(['xvfb-run','-a',str(build/'LSFactoryInstaller_artefacts/Release/LS Factory Setup'),'--self-test',str(payload),str(test)],env=environment,check=True)
    subprocess.run(['/bin/sh',str(test/'program/uninstall.sh')],input='j\n',text=True,env=environment,check=True)
    if (test/'home/Desktop/LSFactory.desktop').exists() or (test/'program/my-project.txt').read_text()!='preserve':raise RuntimeError('Desktop uninstall did not preserve user files or remove its shortcut')
    installer=result=None
    installerDir,installer=appimage(build/'LSFactoryInstaller_artefacts/Release/LS Factory Setup',project/'packaging/linux/installer.desktop','lsfactory-installer')
    artifact=output/f'LSFactory-{version}-linux-x64-Installer.AppImage'
    shutil.copy2(installer,artifact);artifact.chmod(0o755)
    (output/(artifact.name+'.sha256')).write_text(hashlib.sha256(artifact.read_bytes()).hexdigest()+'  '+artifact.name+'\n')
    # Execute the final AppImage and install from its embedded payload, not the staging tree.
    installed=temporary/'final-installer-test'
    subprocess.run(['xvfb-run','-a',str(artifact),'--appimage-extract-and-run','--install-prefix',str(installed)],env=environment,check=True)
    subprocess.run(['xvfb-run','-a',str(installed/'home/.local/bin/lsfactory'),'--export-json',str(temporary/'installed-project.json')],env=environment,check=True)
    if not (temporary/'installed-project.json').exists():raise RuntimeError('Installed workstation failed to run')
    # Exercise the actual generated uninstaller; unrelated files must survive.
    (installed/'program/my-project.txt').write_text('keep')
    subprocess.run(['/bin/sh',str(installed/'program/uninstall.sh')],input='j\n',text=True,env=environment,check=True)
    if (installed/'program/LSFactory.AppImage').exists() or (installed/'home/.local/bin/lsfactory').exists() or (installed/'program/my-project.txt').read_text()!='keep':raise RuntimeError('Uninstall did not preserve unrelated user files')
print(artifact)
