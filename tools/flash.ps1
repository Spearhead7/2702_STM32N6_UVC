# M6 플래시 부팅: 빌드된 앱을 FSBL로 서명하고 DK 외부 NOR 플래시(0x70000000)에 굽는다.
#
# 구조: 앱 하나를 FSBL로 서명한다. 부트 ROM이 플래시에서 AXISRAM2(0x34180400~, 최대 511 KB)로
#       복사해 실행한다. 링커 배치(ROM 383K / RAM 128K)는 개발 모드와 같다.
#       코드가 511 KB를 넘으면 FSBL + 앱(0x70100000) 구조로 바꾼다(docs/progress.md M6).
#
# 사용 (PowerShell, 저장소 루트에서):
#   1) 보드 BOOT1 스위치를 개발 모드(1-3, 핀 순서 2-1-3)로 두고 전원을 다시 넣는다.
#   2) .\tools\flash.ps1            (Debug 빌드, 보드에서 검증된 구성)
#      .\tools\flash.ps1 -Config Release
#      .\tools\flash.ps1 -NoFlash   (서명만)
#      .\tools\flash.ps1 -Image <서명된 파일>   (이미 서명된 이미지를 그대로 굽는다)
#   3) BOOT0·BOOT1 스위치를 모두 1-2(플래시 부팅)로 두고 전원을 다시 넣는다.
param(
    [ValidateSet("Release", "Debug")] [string]$Config = "Debug",
    [switch]$NoFlash,
    [string]$Image = ""
)
$ErrorActionPreference = "Stop"

$cubeProg = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin"
$signTool = Join-Path $cubeProg "STM32_SigningTool_CLI.exe"
$progCli  = Join-Path $cubeProg "STM32_Programmer_CLI.exe"
$loader   = Join-Path $cubeProg "ExternalLoader\MX66UW1G45G_STM32N6570-DK.stldr"

if ($Image -ne "") {
    if (-not (Test-Path $Image)) { throw "이미지가 없습니다: $Image" }
    Write-Host "굽는 이미지: $Image"
    & $progCli -c port=SWD mode=HOTPLUG -el $loader -hardRst -w $Image 0x70000000
    if ($LASTEXITCODE -ne 0) { throw "굽기 실패 (BOOT1이 개발 모드 1-3인지, ST-LINK 연결을 확인하세요)" }
    Write-Host ""
    Write-Host "굽기 완료. BOOT0·BOOT1 스위치를 1-2(플래시 부팅)로 두고 전원을 다시 넣으세요."
    return
}

$outDir  = Join-Path $PSScriptRoot "..\fw\$Config"
$bin     = Join-Path $outDir "STM32N6_UVC.bin"
$trusted = Join-Path $outDir "STM32N6_UVC-trusted.bin"

if (-not (Test-Path $bin)) { throw "빌드 결과가 없습니다: $bin  (CubeIDE에서 $Config 빌드를 먼저 하세요)" }

$size = (Get-Item $bin).Length
Write-Host ("앱 크기: {0:N0} B ({1:N0} KB / 한도 511 KB)" -f $size, ($size / 1024))
if ($size -gt 511KB) { throw "FSBL 한 개로 부팅할 수 있는 크기(511 KB)를 넘었습니다. FSBL + 앱 구조가 필요합니다." }

# CubeProgrammer 2.21 이상은 -align 필요
& $signTool -bin $bin -nk -of 0x80000000 -t fsbl -o $trusted -hv 2.3 -align -s
if ($LASTEXITCODE -ne 0) { throw "서명 실패" }
Write-Host "서명 완료: $trusted"

if ($NoFlash) { return }

& $progCli -c port=SWD mode=HOTPLUG -el $loader -hardRst -w $trusted 0x70000000
if ($LASTEXITCODE -ne 0) { throw "굽기 실패 (BOOT1이 개발 모드 1-3인지, ST-LINK 연결을 확인하세요)" }
Write-Host ""
Write-Host "굽기 완료. BOOT0·BOOT1 스위치를 1-2(플래시 부팅)로 두고 전원을 다시 넣으세요."
