$b = (Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightness).CurrentBrightness
$n = [Math]::Min(100, $b + 10)
$m = Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightnessMethods
Invoke-CimMethod -InputObject $m -MethodName WmiSetBrightness -Arguments @{Timeout=0; Brightness=$n} | Out-Null
