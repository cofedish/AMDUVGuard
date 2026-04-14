$exe  = "d:\Projects\UnderVolageMonitor\AMDUVGuard\bin\Release\AMDUVGuard.exe"
$user = "$env:USERNAME"
$delay = 90
$xml = @"
<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <Author>$user</Author>
    <Description>AMDUVGuard autostart</Description>
  </RegistrationInfo>
  <Triggers>
    <LogonTrigger>
      <Enabled>true</Enabled>
      <Delay>PT${delay}S</Delay>
      <UserId>$user</UserId>
    </LogonTrigger>
  </Triggers>
  <Principals>
    <Principal id="Author">
      <UserId>$user</UserId>
      <LogonType>InteractiveToken</LogonType>
      <RunLevel>LeastPrivilege</RunLevel>
    </Principal>
  </Principals>
  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <AllowHardTerminate>true</AllowHardTerminate>
    <StartWhenAvailable>true</StartWhenAvailable>
    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>
    <IdleSettings>
      <StopOnIdleEnd>false</StopOnIdleEnd>
      <RestartOnIdle>false</RestartOnIdle>
    </IdleSettings>
    <AllowStartOnDemand>true</AllowStartOnDemand>
    <Enabled>true</Enabled>
    <Hidden>false</Hidden>
    <RunOnlyIfIdle>false</RunOnlyIfIdle>
    <WakeToRun>false</WakeToRun>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Priority>7</Priority>
  </Settings>
  <Actions Context="Author">
    <Exec>
      <Command>$exe</Command>
    </Exec>
  </Actions>
</Task>
"@

$tmp = "$env:TEMP\amduvguard_task.xml"
# UTF-16 LE with BOM
[System.IO.File]::WriteAllText($tmp, $xml, [System.Text.Encoding]::Unicode)
Write-Host "Wrote $tmp"
schtasks.exe /Create /F /TN "AMDUVGuard Autostart" /XML $tmp
if ($LASTEXITCODE -eq 0) {
    Write-Host "OK. Task installed."
    Remove-Item $tmp
} else {
    Write-Host "FAILED. XML kept at $tmp"
}
schtasks.exe /Query /TN "AMDUVGuard Autostart"
