param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$version = [regex]::Match((Get-Content "$project/CMakeLists.txt" -Raw), 'project\(LSFactory VERSION ([\d.]+)').Groups[1].Value
$output = (Resolve-Path $OutputDirectory).Path
$archive = Join-Path $output "LSFactory-$version-windows-x64.zip"
$temporary = Join-Path ([System.IO.Path]::GetTempPath()) ("LSFactory-Setup-" + [guid]::NewGuid())
try {
    Expand-Archive $archive $temporary
    $payload = Join-Path $temporary "LSFactory-$version-windows-x64"
    $compiler = "${env:ProgramFiles(x86)}/Inno Setup 6/ISCC.exe"
    if (!(Test-Path $compiler)) { throw 'Inno Setup 6 is required to build the installer' }
    $compilerOutput = & $compiler "/DAppVersion=$version" "/DPayloadPath=$payload" "/DOutputPath=$output" "$project/packaging/windows/setup.iss" 2>&1
    $compilerOutput | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw ("Inno Setup compilation failed: " + ($compilerOutput | Select-Object -Last 12 | Out-String)) }
    $installer = Join-Path $output "LSFactory-$version-windows-x64-Setup.exe"
    if (!(Test-Path $installer)) { throw 'Setup executable was not created' }
    $hash = (Get-FileHash $installer -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([System.IO.Path]::GetFileName($installer))" | Set-Content "$installer.sha256" -Encoding ascii
    # Install silently to a temporary location and run the installed binary.
    $installed = Join-Path $temporary 'installed'
    $process = Start-Process $installer -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', "/DIR=`"$installed`"") -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw "Installer smoke failed: $($process.ExitCode)" }
    $app = Start-Process "$installed/LS Factory.exe" -ArgumentList @('--export-json', "`"$temporary/project.json`"") -PassThru -Wait
    if ($app.ExitCode -ne 0 -or !(Test-Path "$temporary/project.json")) { throw "Installed app smoke failed: exit=$($app.ExitCode), jsonExists=$(Test-Path "$temporary/project.json"), files=$((Get-ChildItem $installed -Name) -join ',')" }
    $smokeDirectory = Join-Path $temporary 'installed-smoke'
    $smokeOut = Join-Path $temporary 'gui-out.log'
    $smokeErr = Join-Path $temporary 'gui-err.log'
    $smoke = Start-Process "$installed/LS Factory.exe" -ArgumentList @('--smoke-test', "`"$smokeDirectory`"") -PassThru -Wait -RedirectStandardOutput $smokeOut -RedirectStandardError $smokeErr
    $smokeDiagnostic = ((Get-Content $smokeOut -Raw), (Get-Content $smokeErr -Raw)) -join "`n"
    Write-Host $smokeDiagnostic
    if ($smoke.ExitCode -ne 0 -or !(Test-Path "$smokeDirectory/playlist.png")) { throw "Installed GUI/WAV smoke failed: exit=$($smoke.ExitCode); $smokeDiagnostic" }
    # Exercise the updater's wait-for-exit path and actual GUI relaunch.
    $userProject = Join-Path $installed 'user-session.odaw'
    'preserve' | Set-Content $userProject
    $oldProcess = Start-Process powershell.exe -ArgumentList @('-NoProfile', '-Command', 'Start-Sleep -Seconds 3') -PassThru
    $updated = Start-Process $installer -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/LSFACTORYUPDATE=1', "/WAITPID=$($oldProcess.Id)", "/DIR=`"$installed`"") -PassThru -Wait
    if ($updated.ExitCode -ne 0 -or (Get-Content $userProject -Raw).Trim() -ne 'preserve') { throw 'Automatic update/preservation smoke failed' }
    $restarted = $null
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        $restarted = Get-Process -Name 'LS Factory' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq "$installed\LS Factory.exe" -and $_.MainWindowHandle -ne 0 } | Select-Object -First 1
        if ($restarted) { break }
        Start-Sleep -Milliseconds 200
    }
    if (!$restarted) { throw 'Updated app did not relaunch' }
    $restarted.CloseMainWindow() | Out-Null
    if (!$restarted.WaitForExit(30000)) { $restarted.Kill(); throw 'Updated app did not close cleanly' }
    $uninstaller = Start-Process "$installed/unins000.exe" -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') -PassThru -Wait
    if ($uninstaller.ExitCode -ne 0) { throw 'Uninstall smoke failed' }
    Write-Host $installer
} catch {
    # Public annotations preserve useful failure evidence when CI logs require sign-in.
    $diagnostic = $_.Exception.Message.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
    Write-Output "::error title=Windows installer packaging::$diagnostic"
    throw
} finally {
    Remove-Item $temporary -Recurse -Force -ErrorAction SilentlyContinue
}
