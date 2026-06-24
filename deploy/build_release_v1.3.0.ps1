# EmberInterDebugTool v1.3.0 一键打包脚本 (PowerShell)
# 用法: powershell -ExecutionPolicy Bypass -File deploy\build_release_v1.3.0.ps1
# 需求: MSYS2 + Inno Setup 6

$ErrorActionPreference = "Stop"
$ProjectDir = (Resolve-Path "$PSScriptRoot\..").Path
$MsysBash = "C:\msys64\usr\bin\bash.exe"
$Iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

Write-Host "================================================" -ForegroundColor Cyan
Write-Host "  EmberInterDebugTool v1.3.0 Release 打包" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

# 检查依赖
if (-not (Test-Path $MsysBash)) {
    Write-Host "错误: 未找到 MSYS2 bash ($MsysBash)" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $Iscc)) {
    Write-Host "错误: 未找到 Inno Setup 6 ($Iscc)" -ForegroundColor Red
    Write-Host "请从 https://jrsoftware.org/isdl.php 安装 Inno Setup 6" -ForegroundColor Yellow
    exit 1
}

# 步骤 1: 在 MSYS2 中构建 Release 并收集 DLL
Write-Host "[1/2] MSYS2 Release 构建与依赖收集..." -ForegroundColor Green
$bashArgs = "-lc 'export PATH=/c/msys64/mingw64/bin:`$PATH && cd /f/work/software/serial-monitor && bash deploy/release_v1.3.0.sh'"
$process = Start-Process -FilePath $MsysBash -ArgumentList "-lc", "export PATH=/c/msys64/mingw64/bin:`$PATH && cd /f/work/software/serial-monitor && bash deploy/release_v1.3.0.sh" -Wait -PassThru -NoNewWindow
if ($process.ExitCode -ne 0) {
    Write-Host "错误: MSYS2 构建失败 (退出码 $($process.ExitCode))" -ForegroundColor Red
    exit 1
}
Write-Host "MSYS2 构建完成" -ForegroundColor Green
Write-Host ""

# 步骤 2: Inno Setup 编译安装包
Write-Host "[2/2] Inno Setup 编译安装包..." -ForegroundColor Green
$issPath = Join-Path $ProjectDir "deploy\emberInter.iss"
Set-Location $ProjectDir
& $Iscc $issPath
if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: Inno Setup 编译失败 (退出码 $LASTEXITCODE)" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "  打包完成!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Cyan
$distDir = Join-Path $ProjectDir "dist"
if (Test-Path $distDir) {
    Write-Host "安装包目录: $distDir"
    Get-ChildItem $distDir | ForEach-Object {
        $sizeMB = [math]::Round($_.Length / 1MB, 2)
        Write-Host "  $($_.Name)  ($sizeMB MB)"
    }
}
Write-Host ""
Write-Host "可将安装包上传到 GitHub Release:" -ForegroundColor Yellow
Write-Host "  https://github.com/kukucaiCndy/serial-monitor/releases/new?tag=v1.3.0" -ForegroundColor Yellow
