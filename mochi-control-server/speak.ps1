param([switch]$Probe)
$ErrorActionPreference = 'Stop'
[Console]::InputEncoding = [Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
Add-Type -AssemblyName System.Speech
$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
try {
  $russian = @($synth.GetInstalledVoices() | Where-Object { $_.Enabled -and $_.VoiceInfo.Culture.Name -like 'ru-*' })
  if ($russian.Count -eq 0) { throw 'No Russian Windows voice installed' }
  $synth.SelectVoice($russian[0].VoiceInfo.Name)
  if ($Probe) {
    @{ voice = $russian[0].VoiceInfo.Name; available = $true } | ConvertTo-Json -Compress
    return
  }
  $request = [Console]::In.ReadToEnd() | ConvertFrom-Json
  $text = [string]$request.text
  if ($text.Length -gt 1000) { throw 'Speech text is too long' }
  $synth.SetOutputToDefaultAudioDevice()
  $synth.Rate = 0
  $synth.Volume = 85
  # Speak treats text as plain text, never as PowerShell code or SSML.
  $synth.Speak($text)
} finally { $synth.Dispose() }
