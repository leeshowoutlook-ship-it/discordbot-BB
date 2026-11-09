# dcbot watchdog: restarts the bot when it is not running.
# While updating the bot, create "watchdog.pause" in this folder to pause it; delete the file afterwards.
$dir   = "C:\Users\USER2021\discordbot-BB\build\Release"
$log   = Join-Path $dir "watchdog.log"
$pause = Join-Path $dir "watchdog.pause"

Add-Content -Path $log -Value "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') - watchdog started (PID $PID)"
while ($true) {
    if (-not (Test-Path $pause)) {
        $proc = Get-Process -Name "dcbot" -ErrorAction SilentlyContinue
        if (-not $proc -and (Test-Path (Join-Path $dir "dcbot.exe"))) {
            $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            Add-Content -Path $log -Value "$ts - dcbot not running, restarting..."
            # keep the previous run's logs for crash diagnosis
            foreach ($f in "bot_err.txt", "bot_out.txt") {
                $p = Join-Path $dir $f
                if (Test-Path $p) { Copy-Item $p (Join-Path $dir ($f -replace '\.txt$', '.prev.txt')) -Force }
            }
            Start-Process -FilePath (Join-Path $dir "dcbot.exe") `
                -WorkingDirectory $dir `
                -RedirectStandardOutput (Join-Path $dir "bot_out.txt") `
                -RedirectStandardError (Join-Path $dir "bot_err.txt") `
                -WindowStyle Hidden
            Start-Sleep -Seconds 30
        }
    }
    Start-Sleep -Seconds 15
}
