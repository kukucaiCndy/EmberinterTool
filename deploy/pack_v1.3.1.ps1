$ErrorActionPreference = "Stop"
$ProjectDir = "F:\work\software\serial-monitor"
$DeployDir = "$ProjectDir\deploy\emberInter"
$DistDir = "$ProjectDir\dist"
$MingwBin = "C:\msys64\mingw64\bin"
$Objdump = "$MingwBin\objdump.exe"
$Iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
$BuildDir = "$ProjectDir\build-release"

Write-Host "================================================"
Write-Host "  EmberInterDebugTool v1.3.1"
Write-Host "================================================"

# 清理
if (Test-Path $DeployDir) { Remove-Item $DeployDir -Recurse -Force }
if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
New-Item -ItemType Directory -Path $DeployDir -Force | Out-Null
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null

# 复制 exe
Copy-Item "$BuildDir\bin\serial-monitor.exe" $DeployDir
Copy-Item "$BuildDir\bin\serial-monitor-cli.exe" $DeployDir
Write-Host "[OK] exe copied"

# 收集 DLL
$SystemDlls = @("kernel32.dll","user32.dll","gdi32.dll","advapi32.dll","shell32.dll","comdlg32.dll","ole32.dll","oleaut32.dll","winspool.drv","comctl32.dll","winmm.dll","ws2_32.dll","rpcrt4.dll","imm32.dll","msvcrt.dll","shlwapi.dll","uxtheme.dll","dwmapi.dll","version.dll","setupapi.dll","cfgmgr32.dll","secur32.dll","crypt32.dll","wldap32.dll","dnsapi.dll","iphlpapi.dll","bcrypt.dll","ncrypt.dll","powrprof.dll","profapi.dll","netapi32.dll","mpr.dll","userenv.dll","dxgi.dll","d3d11.dll","usp10.dll","dwrite.dll")
function Get-Deps($p) { $o = & $Objdump -p $p 2>&1; $r = @(); foreach ($l in $o) { if ($l -match "DLL Name:\s*(\S+)") { $r += $matches[1] } }; return $r }
function Copy-Rec($t, $c) {
    foreach ($d in (Get-Deps $t)) {
        $lo = $d.ToLower()
        if ($SystemDlls -contains $lo -or $lo -match "^(msvcp|vcruntime)") { continue }
        if ($c.Contains($lo)) { continue }
        $s = Join-Path $MingwBin $d
        if (-not (Test-Path $s)) { $f = Get-ChildItem "C:\msys64\mingw64" -Filter $d -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1; if ($f) { $s = $f.FullName } }
        if (Test-Path $s) { Copy-Item $s (Join-Path $DeployDir $d) -Force; $c.Add($lo) | Out-Null; Write-Host "  DLL: $d"; Copy-Rec $s $c }
    }
}
$copied = New-Object System.Collections.Generic.HashSet[string]
Copy-Rec "$BuildDir\bin\serial-monitor.exe" $copied
Copy-Rec "$BuildDir\bin\serial-monitor-cli.exe" $copied

# Qt plugins
New-Item -ItemType Directory -Path "$DeployDir\platforms" -Force | Out-Null
Copy-Item "C:\msys64\mingw64\share\qt6\plugins\platforms\qwindows.dll" "$DeployDir\platforms\" -Force
Copy-Item "C:\msys64\mingw64\share\qt6\plugins\platforms\qdirect2d.dll" "$DeployDir\platforms\" -Force -ErrorAction SilentlyContinue
Copy-Item "C:\msys64\mingw64\share\qt6\plugins\platforms\qminimal.dll" "$DeployDir\platforms\" -Force -ErrorAction SilentlyContinue
Copy-Item "C:\msys64\mingw64\share\qt6\plugins\platforms\qoffscreen.dll" "$DeployDir\platforms\" -Force -ErrorAction SilentlyContinue

New-Item -ItemType Directory -Path "$DeployDir\imageformats" -Force | Out-Null
Get-ChildItem "C:\msys64\mingw64\share\qt6\plugins\imageformats\*.dll" | ForEach-Object { Copy-Item $_.FullName "$DeployDir\imageformats\" -Force }

if (Test-Path "C:\msys64\mingw64\share\qt6\qml") { Copy-Item "C:\msys64\mingw64\share\qt6\qml" "$DeployDir\qml" -Recurse -Force }

New-Item -ItemType Directory -Path "$DeployDir\icons" -Force | Out-Null
Copy-Item "$ProjectDir\resources\icons\logo.png" "$DeployDir\icons\" -ErrorAction SilentlyContinue
Copy-Item "$ProjectDir\resources\icons\app.ico" "$DeployDir\icons\"
Set-Content "$DeployDir\emberInter.bat" "@echo off`r`nchcp 65001 > nul`r`nset PATH=%~dp0;%~dp0platforms;%PATH%`r`nstart "" ""%~dp0serial-monitor.exe"""
Set-Content "$DeployDir\emberInter-cli.bat" "@echo off`r`nchcp 65001 > nul`r`nset PATH=%~dp0;%~dp0platforms;%PATH%`r`n""%~dp0serial-monitor-cli.exe"" %*"

Write-Host "[OK] DLL collected: $((Get-ChildItem "$DeployDir\*.dll").Count)"

# Inno Setup
Set-Location $ProjectDir
& $Iscc "deploy\emberInter.iss" 2>&1 | Select-Object -Last 3

Write-Host ""
$setup = Get-ChildItem "$DistDir\*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($setup) { Write-Host "[DONE] $($setup.Name) ($([math]::Round($setup.Length/1MB,2)) MB)" -ForegroundColor Green }
