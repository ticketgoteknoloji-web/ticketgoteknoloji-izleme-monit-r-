# Builds TicketGo Izleme Windows x64 package + Inno Setup installer.
# Requires: CMake, Ninja or MSVC, Qt 6.5+, windeployqt, Inno Setup 6.
param(
  [string]$BuildDir = "build-windows",
  [string]$QtPrefix = $env:QT_PREFIX,
  [string]$InnoCompiler = $env:INNO_SETUP_COMPILER
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $Root

if (-not $QtPrefix -or -not (Test-Path $QtPrefix)) {
  throw "QT_PREFIX must point to a Qt 6 kit (example: C:\Qt\6.8.0\msvc2022_64)"
}
$Windeploy = Join-Path $QtPrefix "bin\windeployqt.exe"
if (-not (Test-Path $Windeploy)) { throw "windeployqt not found at $Windeploy" }

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
cmake -S . -B $BuildDir -G "Ninja" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtPrefix"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
cmake --build $BuildDir --config Release --target ops_core_tests update_tests ticketgo-izleme ticketgo-update-helper
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

& "$BuildDir\ops_core_tests.exe"
if ($LASTEXITCODE -ne 0) { throw "ops_core_tests failed" }
& "$BuildDir\update_tests.exe"
if ($LASTEXITCODE -ne 0) { throw "update_tests failed" }

$AppExe = Join-Path $BuildDir "TicketGoIzleme.exe"
if (-not (Test-Path $AppExe)) { throw "TicketGoIzleme.exe missing" }
& $AppExe --probe-once
if ($LASTEXITCODE -ne 0) { throw "smoke probe failed" }

$Dist = Join-Path $BuildDir "dist"
if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
New-Item -ItemType Directory -Force -Path $Dist | Out-Null
Copy-Item $AppExe $Dist
$Helper = Join-Path $BuildDir "ticketgo-update-helper.exe"
if (Test-Path $Helper) { Copy-Item $Helper $Dist }

& $Windeploy (Join-Path $Dist "TicketGoIzleme.exe") --qmldir (Join-Path $Root "qml") --release --no-translations
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }
if (Test-Path $Helper) {
  Copy-Item $Helper (Join-Path $Dist "ticketgo-update-helper.exe") -Force
}

if (-not $InnoCompiler) {
  $candidates = @(
    "${env:LocalAppData}\Programs\Inno Setup 6\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
  )
  $InnoCompiler = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $InnoCompiler -or -not (Test-Path $InnoCompiler)) {
  throw "Inno Setup compiler (ISCC.exe) not found. Set INNO_SETUP_COMPILER."
}

$Iss = Join-Path $Root "packaging\windows\TicketGoIzleme.iss"
& $InnoCompiler "/DBuildDir=$Dist" "/DOutputDir=$BuildDir" $Iss
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed" }

$Setup = Join-Path $BuildDir "TicketGoIzleme-Windows-x64-Setup.exe"
if (-not (Test-Path $Setup)) { throw "installer missing: $Setup" }
$Hash = (Get-FileHash -Algorithm SHA256 $Setup).Hash.ToLowerInvariant()
$Size = (Get-Item $Setup).Length
$Manifest = @{
  channel = "stable"
  version = "0.1.3"
  assets = @(
    @{
      name = "TicketGoIzleme-Windows-x64-Setup.exe"
      platform = "windows"
      arch = "x64"
      sha256 = $Hash
      size = $Size
      bundleId = "tr.ticketgoteknoloji.izleme"
    }
  )
} | ConvertTo-Json -Depth 5
$ManifestPath = Join-Path $BuildDir "TicketGoIzleme-update-manifest.windows.json"
Set-Content -Path $ManifestPath -Value $Manifest -Encoding utf8
$ShaPath = Join-Path $BuildDir "TicketGoIzleme-Windows-x64-Setup.exe.sha256"
Set-Content -Path $ShaPath -Value "TicketGoIzleme-Windows-x64-Setup.exe  $Hash" -Encoding ascii
Write-Host "INSTALLER=$Setup"
Write-Host "SHA256=$Hash"
Write-Host "SHA256_FILE=$ShaPath"
Write-Host "MANIFEST=$ManifestPath"
Write-Host "SIGNING=unsigned (set WINDOWS_CODE_SIGN_* secrets only when a certificate exists)"
