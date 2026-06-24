# 手动收集 DLL 依赖并复制到部署目录
$ErrorActionPreference = "Stop"
$ProjectDir = (Resolve-Path "$PSScriptRoot\..").Path
$BuildDir = Join-Path $ProjectDir "build-release"
$DeployDir = Join-Path $ProjectDir "deploy\emberInter"
$MingwBin = "C:\msys64\mingw64\bin"
$Objdump = Join-Path $MingwBin "objdump.exe"

Write-Host "=== 收集 DLL 依赖 ===" -ForegroundColor Green

# 系统 DLL 白名单 (不复制)
$SystemDlls = @(
    "kernel32.dll","user32.dll","gdi32.dll","advapi32.dll","shell32.dll",
    "comdlg32.dll","ole32.dll","oleaut32.dll","winspool.drv","comctl32.dll",
    "winmm.dll","ws2_32.dll","rpcrt4.dll","imm32.dll","msvcrt.dll","shlwapi.dll",
    "uxtheme.dll","dwmapi.dll","version.dll","setupapi.dll","cfgmgr32.dll",
    "secur32.dll","crypt32.dll","wldap32.dll","dnsapi.dll","iphlpapi.dll",
    "bcrypt.dll","ncrypt.dll","powrprof.dll","profapi.dll","netapi32.dll","mpr.dll",
    "userenv.dll","dxgi.dll","d3d11.dll","usp10.dll","dwrite.dll"
)

function Get-DllDependencies($exePath) {
    $output = & $Objdump -p $exePath 2>&1
    $dlls = @()
    foreach ($line in $output) {
        if ($line -match "DLL Name:\s*(\S+)") {
            $dlls += $matches[1]
        }
    }
    return $dlls
}

function Copy-DllRecursive($target, $deployDir, $copiedSet) {
    $dlls = Get-DllDependencies $target
    foreach ($dll in $dlls) {
        $lower = $dll.ToLower()
        if ($SystemDlls -contains $lower) { continue }
        if ($lower -match "^(msvcp|vcruntime)") { continue }
        if ($copiedSet.Contains($lower)) { continue }

        $src = Join-Path $MingwBin $dll
        if (-not (Test-Path $src)) {
            # 在 mingw64 目录递归查找
            $found = Get-ChildItem "C:\msys64\mingw64" -Filter $dll -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($found) { $src = $found.FullName }
        }

        if (Test-Path $src) {
            $dst = Join-Path $deployDir $dll
            Copy-Item $src $dst -Force
            $copiedSet.Add($lower) | Out-Null
            Write-Host "  复制: $dll"
            # 递归解析依赖
            Copy-DllRecursive $src $deployDir $copiedSet
        }
    }
}

$copied = New-Object System.Collections.Generic.HashSet[string]

# 收集 GUI 依赖
Write-Host "GUI (serial-monitor.exe):"
Copy-DllRecursive (Join-Path $BuildDir "bin\serial-monitor.exe") $DeployDir $copied

# 收集 CLI 依赖
Write-Host "CLI (serial-monitor-cli.exe):"
Copy-DllRecursive (Join-Path $BuildDir "bin\serial-monitor-cli.exe") $DeployDir $copied

# 复制 Qt 平台插件
Write-Host "`n=== 复制 Qt 插件 ===" -ForegroundColor Green
$platformsDir = Join-Path $DeployDir "platforms"
New-Item -ItemType Directory -Path $platformsDir -Force | Out-Null
Copy-Item "C:\msys64\mingw64\share\qt6\plugins\platforms\qwindows.dll" $platformsDir -Force
Write-Host "  复制: platforms/qwindows.dll"
Copy-Item "C:\msys64\mingw64\share\qt6\plugins\platforms\qdirect2d.dll" $platformsDir -Force -ErrorAction SilentlyContinue
Copy-Item "C:\msys64\mingw64\share\qt6\plugins\platforms\qminimal.dll" $platformsDir -Force -ErrorAction SilentlyContinue
Copy-Item "C:\msys64\mingw64\share\qt6\plugins\platforms\qoffscreen.dll" $platformsDir -Force -ErrorAction SilentlyContinue

# 复制图像格式插件
$imageformatsDir = Join-Path $DeployDir "imageformats"
New-Item -ItemType Directory -Path $imageformatsDir -Force | Out-Null
Get-ChildItem "C:\msys64\mingw64\share\qt6\plugins\imageformats\*.dll" | ForEach-Object {
    Copy-Item $_.FullName $imageformatsDir -Force
}
Write-Host "  复制: imageformats ($((Get-ChildItem $imageformatsDir).Count) 文件)"

# 复制 QML 模块
Write-Host "`n=== 复制 QML 模块 ===" -ForegroundColor Green
$qmlSrcDir = "C:\msys64\mingw64\share\qt6\qml"
$qmlDstDir = Join-Path $DeployDir "qml"
if (Test-Path $qmlSrcDir) {
    Copy-Item $qmlSrcDir $qmlDstDir -Recurse -Force
    $qmlCount = (Get-ChildItem $qmlDstDir -Recurse -File).Count
    Write-Host "  复制: QML 模块 ($qmlCount 文件)"
}

# 复制 styles
$stylesSrcDir = "C:\msys64\mingw64\share\qt6\plugins\styles"
$stylesDstDir = Join-Path $DeployDir "styles"
if (Test-Path $stylesSrcDir) {
    New-Item -ItemType Directory -Path $stylesDstDir -Force | Out-Null
    Copy-Item "$stylesSrcDir\*" $stylesDstDir -Force -Recurse
    Write-Host "  复制: styles"
}

# 统计结果
Write-Host "`n=== 部署结果 ===" -ForegroundColor Green
$dllCount = (Get-ChildItem "$DeployDir\*.dll").Count
Write-Host "DLL 数量: $dllCount"
Write-Host "子目录:"
Get-ChildItem $DeployDir -Directory | ForEach-Object {
    $count = (Get-ChildItem $_.FullName -Recurse -File).Count
    Write-Host "  $($_.Name)/ ($count 文件)"
}
Write-Host "`n部署目录准备完成: $DeployDir" -ForegroundColor Green
