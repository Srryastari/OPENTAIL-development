# Read-only ESP32-S3 USB serial logger. Never transmits commands or toggles BOOT.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][ValidatePattern('^COM[0-9]{1,3}$')][string]$Port,
 [Parameter(Mandatory=$true)][string]$OutFile,
 [ValidateRange(1,3600)][int]$Seconds=30,
 [string]$TimelineFile="",
 [string]$StopFile=""
)
Set-StrictMode -Version Latest
$ErrorActionPreference="Stop"
$sp=[System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$sp.Handshake=[System.IO.Ports.Handshake]::None
$sp.DtrEnable=$false
$sp.RtsEnable=$false
$sp.ReadTimeout=250
$sp.Encoding=[System.Text.Encoding]::UTF8
$writer=[System.IO.StreamWriter]::new($OutFile,$false,[System.Text.UTF8Encoding]::new($false))
$timeline=$null
$partial=""
try {
 if($TimelineFile){$timeline=[System.IO.StreamWriter]::new($TimelineFile,$false,[System.Text.UTF8Encoding]::new($false))}
 $sp.Open()
 $writer.WriteLine("[OpenTail] USB SERIAL LOG START "+(Get-Date).ToString("o")+" port="+$Port+" baud=115200; receive-only")
 $writer.Flush()
 $timer=[System.Diagnostics.Stopwatch]::StartNew()
 while($timer.Elapsed.TotalSeconds -lt $Seconds) {
  if($StopFile -and (Test-Path -LiteralPath $StopFile -PathType Leaf)){break}
  if($sp.BytesToRead -gt 0) {
   $chunk=$sp.ReadExisting()
   $writer.Write($chunk)
   $writer.Flush()
   if($null -ne $timeline){
    $received=(Get-Date).ToUniversalTime().ToString('o')
    $segments=@(($partial+$chunk) -split '\r?\n')
    for($i=0;$i -lt ($segments.Count-1);$i++){
     $timeline.WriteLine($received+" | "+([string]$segments[$i]).TrimEnd([char]13))
    }
    $partial=[string]$segments[$segments.Count-1]
    $timeline.Flush()
   }
  }
  Start-Sleep -Milliseconds 120
 }
 if($null -ne $timeline -and $partial.Length -gt 0){
  $timeline.WriteLine((Get-Date).ToUniversalTime().ToString('o')+" | "+$partial)
  $timeline.Flush()
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
 if($null -ne $timeline){$timeline.Dispose()}
}
