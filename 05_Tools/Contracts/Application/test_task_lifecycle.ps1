$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        $failures.Add($Message)
    }
}

function Read-TaskEntry {
    param(
        [string]$Path,
        [string]$FunctionName
    )

    $content = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    $pattern = "(?s)static\s+void\s+{0}\s*\([^)]*\)\s*\{{(.*?)(?=^\}}\s*$)" -f [regex]::Escape($FunctionName)
    $match = [regex]::Match($content, $pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if (-not $match.Success) {
        throw "Task entry was not found: $FunctionName"
    }

    return $match.Groups[1].Value
}

$threadAdapterPath = Join-Path $repoRoot "03_Firmware\Application\OTA_APP\04_Impl\impl_os\freertos\impl_freertos_thread.c"
$taskCases = @(
    @{ Path = (Join-Path $repoRoot "03_Firmware\Application\OTA_APP\01_APP\task\app_main_task.c"); Function = "app_main_task_entry" },
    @{ Path = (Join-Path $repoRoot "03_Firmware\Application\OTA_APP\01_APP\task\app_ota_worker.c"); Function = "app_ota_worker_entry" },
    @{ Path = (Join-Path $repoRoot "03_Firmware\Application\OTA_APP\01_APP\task\app_display_task.c"); Function = "app_display_task_entry" }
)

Assert-True (Test-Path -LiteralPath $threadAdapterPath -PathType Leaf) "FreeRTOS thread adapter is missing"
if (Test-Path -LiteralPath $threadAdapterPath -PathType Leaf) {
    $adapter = Get-Content -LiteralPath $threadAdapterPath -Raw -Encoding UTF8
    Assert-True ($adapter -match "osThreadGetId") "Thread termination must recognize the current Task"
}

foreach ($case in $taskCases) {
    Assert-True (Test-Path -LiteralPath $case.Path -PathType Leaf) "Task source is missing: $($case.Path)"
    if (Test-Path -LiteralPath $case.Path -PathType Leaf) {
        try {
            $entry = Read-TaskEntry -Path $case.Path -FunctionName $case.Function
            $terminateFunction = $case.Function -replace "_entry$", "_terminate"
            Assert-True ($entry -match [regex]::Escape($terminateFunction)) "$($case.Function) must terminate safely through its Task lifecycle helper"
            Assert-True ($entry -notmatch "(?m)^\s*return\s*;\s*$") "$($case.Function) must not return directly from a RTOS Task entry"

            $helperPattern = "(?s)static\s+void\s+{0}\s*\(\s*void\s*\)\s*\{{(.*?)(?=^\}}\s*$)" -f [regex]::Escape($terminateFunction)
            $helperMatch = [regex]::Match((Get-Content -LiteralPath $case.Path -Raw -Encoding UTF8), $helperPattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
            Assert-True $helperMatch.Success "$terminateFunction helper is missing"
            if ($helperMatch.Success) {
                Assert-True ($helperMatch.Groups[1].Value -match "platform_thread_terminate") "$terminateFunction must use Platform Thread termination"
                Assert-True ($helperMatch.Groups[1].Value -match "platform_time_delay_ms") "$terminateFunction must not return if termination unexpectedly returns"
            }
        }
        catch {
            $failures.Add($_.Exception.Message)
        }
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { "[TaskLifecycle][FAIL] $_" }
    exit 1
}

"[TaskLifecycle][PASS] Long-lived Task safe-exit contracts passed."
exit 0
