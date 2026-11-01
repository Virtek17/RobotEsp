$ErrorActionPreference = 'Stop'
[Console]::InputEncoding = [Text.UTF8Encoding]::new($false)
$request = [Console]::In.ReadToEnd() | ConvertFrom-Json
$bytes = [Convert]::FromBase64String([string]$request.wav)
$stream = [IO.MemoryStream]::new($bytes, $false)
$player = [System.Media.SoundPlayer]::new($stream)
try { $player.Load(); $player.PlaySync() }
finally { $player.Dispose(); $stream.Dispose() }
