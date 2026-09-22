while ($true) {
    $proc = Get-Process -Name "dcbot" -ErrorAction SilentlyContinue
    if (-not $proc) {
        $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        Add-Content -Path "C:\Users\USER2021\discordbot-BB\build\Release\watchdog.log" -Value "$ts - dcbot not running, restarting..."
        Start-Process -FilePath "C:\Users\USER2021\discordbot-BB\build\Release\dcbot.exe" `
            -WorkingDirectory "C:\Users\USER2021\discordbot-BB\build\Release" `
            -RedirectStandardOutput "C:\Users\USER2021\discordbot-BB\build\Release\stdout.log" `
            -RedirectStandardError "C:\Users\USER2021\discordbot-BB\build\Release\stderr.log"
    }
    Start-Sleep -Seconds 15
}
