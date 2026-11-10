# WT_FreeRTOS 一键格式化脚本
# 1. 用 clang-format 按工程配置格式化
# 2. 保证每个文件最后一行是空行（clang-format 会删末尾空行，这里补回来）
#
# 用法（在工程根目录）：
#   .\format.ps1                 # 默认格式化 app/ 和 driver/
#   .\format.ps1 -Paths app      # 只格式化 app/
# 如果被执行策略挡住：
#   powershell -ExecutionPolicy Bypass -File .\format.ps1

param(
    [string[]]$Paths = @('app', 'driver')
)

$root = $PSScriptRoot
$enc = New-Object System.Text.UTF8Encoding($false)

$files = foreach ($p in $Paths) {
    Get-ChildItem -LiteralPath (Join-Path $root $p) -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in '.c', '.h' }
}

$count = 0
foreach ($f in $files) {
    # 第一步：clang-format
    clang-format -i --style=file $f.FullName
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "clang-format 失败: $($f.FullName)"
        continue
    }

    # 第二步：文件末尾统一为"一个空行"（去掉多余空行后补两个换行）
    $text = [System.IO.File]::ReadAllText($f.FullName)
    $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $normalized = $text.TrimEnd("`r", "`n") + $nl + $nl
    if ($normalized -ne $text) {
        [System.IO.File]::WriteAllText($f.FullName, $normalized, $enc)
    }
    $count++
}

Write-Output "已处理 $count 个文件（目录: $($Paths -join ', ')）"