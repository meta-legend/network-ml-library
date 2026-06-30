# Produces the self-contained Windows release binaries: builds the library
# with MSVC + vcpkg, then merges libcurl + zlib into networkml.lib using
# lib.exe so a consumer links only networkml.lib (the Windows system libs
# auto-link via #pragma comment(lib, ...) in the source).
#
# Usage (from the repo root, in any shell - vcvars is invoked internally):
#   pwsh tools/make-release-lib.ps1
#
# Output:
#   build/release-bundle/
#     networkml.h
#     lib/networkml.lib   (Release, /MD)
#     lib/networkmld.lib  (Debug,   /MDd)

[CmdletBinding()]
param(
    [string]$Triplet = "x64-windows-static-md",
    [string]$VcpkgRoot,
    [string]$OutDir = "build/release-bundle"
)

$ErrorActionPreference = "Stop"

# ---- locate tools ------------------------------------------------------------
$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $repoRoot

if (-not $VcpkgRoot) {
    # Prefer a standalone vcpkg checkout (D:\vcpkg) over the VS-bundled one,
    # because the bundled vcpkg only ships the runtime, not the port tree.
    if (Test-Path "D:\vcpkg\vcpkg.exe") { $VcpkgRoot = "D:\vcpkg" }
    elseif ($env:VCPKG_ROOT) { $VcpkgRoot = $env:VCPKG_ROOT }
    else { throw "VCPKG_ROOT not set and D:\vcpkg not found. Pass -VcpkgRoot." }
}

# Find vcvars64.bat (needed so lib.exe is on PATH and points at the MSVC arch).
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found; install Visual Studio." }
$vsRoot = & $vswhere -latest -property installationPath
$vcvars = "$vsRoot\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }

# CMake (prefer VS's bundled copy so PATH-less invocations work).
$cmakeExe = $null
foreach ($p in @(
    "$vsRoot\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "${env:ProgramFiles}\CMake\bin\cmake.exe"
)) { if (Test-Path $p) { $cmakeExe = $p; break } }
if (-not $cmakeExe) {
    $c = Get-Command cmake -ErrorAction SilentlyContinue
    if ($c) { $cmakeExe = $c.Source }
}
if (-not $cmakeExe) { throw "cmake.exe not found." }

Write-Host "[1/4] Installing static-md curl + nlohmann-json via vcpkg..."
& "$VcpkgRoot\vcpkg.exe" install "curl:$Triplet" "nlohmann-json:$Triplet" | Out-Host

# ---- build Release and Debug with CMake (Visual Studio generator, MSVC) -----
function Build-Config([string]$cfg) {
    Write-Host "[2/4] Configuring + building $cfg..."
    $b = "build/lib-$cfg"
    & $cmakeExe -B $b -S . `
        -G "Visual Studio 18 2026" -A x64 `
        "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake" `
        "-DVCPKG_TARGET_TRIPLET=$Triplet" | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($cfg)" }
    & $cmakeExe --build $b --config $cfg | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed ($cfg)" }
    return "$repoRoot\$b\$cfg\networkml.lib"
}
$relLib = Build-Config "Release"
$dbgLib = Build-Config "Debug"

# ---- merge libcurl + zlib into networkml.lib via lib.exe --------------------
function Merge-Lib([string]$inLib, [string]$triplet, [bool]$debug, [string]$outLib) {
    Write-Host "[3/4] Merging libcurl + zlib into $(Split-Path $outLib -Leaf)..."
    $libsDir = if ($debug) { "$VcpkgRoot\installed\$triplet\debug\lib" } else { "$VcpkgRoot\installed\$triplet\lib" }
    $curl    = if ($debug) { "$libsDir\libcurl-d.lib" } else { "$libsDir\libcurl.lib" }
    $zlib    = if ($debug) { "$libsDir\zsd.lib" }      else { "$libsDir\zs.lib" }
    foreach ($p in @($curl, $zlib)) { if (-not (Test-Path $p)) { throw "missing $p" } }

    # Invoke lib.exe from inside a vcvars64 environment so it's on PATH.
    $cmd = "`"$vcvars`" >nul 2>&1 && lib.exe /NOLOGO /OUT:`"$outLib`" `"$inLib`" `"$curl`" `"$zlib`""
    cmd /c $cmd | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "lib.exe merge failed" }
}

Write-Host "[4/4] Staging output in $OutDir..."
$stage = Join-Path $repoRoot $OutDir
New-Item -ItemType Directory -Force -Path "$stage\lib" | Out-Null
Copy-Item "Network ML\networkml.h" "$stage\networkml.h" -Force

Merge-Lib $relLib $Triplet $false "$stage\lib\networkml.lib"
Merge-Lib $dbgLib $Triplet $true  "$stage\lib\networkmld.lib"

Write-Host ""
Write-Host "Done. Self-contained binaries:"
Get-ChildItem "$stage" -Recurse -File | ForEach-Object {
    "{0,-50}  {1,8:N1} MB" -f $_.FullName.Substring($repoRoot.Length+1), ($_.Length/1MB)
}
