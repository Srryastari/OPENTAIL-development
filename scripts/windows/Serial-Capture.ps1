# Read-only ESP32-S3 USB serial logger. Never transmits commands or toggles BOOT.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][ValidatePattern('^COM[0-9]{1,3}$')][string]$Port,
 [Parameter(Mandatory=$true)][string]$OutFile,
 [ValidateRange(1,3600)][int]$Seconds=120
)
Set-StrictMode -Version Latest
$ErrorActionPreference="Stop"
$sp=New-Object System.IO.Ports.SerialPort($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$sp.Handshake=[System.IO.Ports.Handshake]::None
$sp.DtrEnable=$false
$sp.RtsEnable=$false
$sp.ReadTimeout=250
$sp.Encoding=[System.Text.Encoding]::UTF8
$writer=New-Object System.IO.StreamWriter($OutFile,$false,(New-Object System.Text.UTF8Encoding($false)))
try {
 $sp.Open()
 $writer.WriteLine("[OpenTail] USB SERIAL LOG START "+(Get-Date).ToString("o")+" port="+$Port+" baud=115200; receive-only")
 $writer.Flush()
 $timer=[System.Diagnostics.Stopwatch]::StartNew()
 while($timer.Elapsed.TotalSeconds -lt $Seconds) {
  if($sp.BytesToRead -gt 0) {
   $writer.Write($sp.ReadExisting())
   $writer.Flush()
  }
  Start-Sleep -Milliseconds 120
 }
 $writer.WriteLine("")
 $writer.WriteLine("[OpenTail] USB SERIAL LOG END "+(Get-Date).ToString("o"))
 $writer.Flush()
} catch {
 $writer.WriteLine("[OpenTail] USB SERIAL CAPTURE ERROR: "+$_.Exception.Message)
 $writer.Flush()
 throw
} finally {
 if($sp.IsOpen){$sp.Close()}
 $sp.Dispose()
 $writer.Dispose()
}
