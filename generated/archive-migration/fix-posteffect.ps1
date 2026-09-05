$ErrorActionPreference = 'Stop'
$target = 'C:\project\MyEngine\project\Engine\PostEffect\PostEffectManager.cpp'
$content = [IO.File]::ReadAllText($target)
$start = $content.IndexOf('        // ArchiveAtmosphere uses the existing reserved slots;')
if ($start -lt 0) { throw 'Expected duplicate initialization block missing' }
$end = $content.IndexOf('postEffectParameter.radialBlurCenter = { 0.5f, 0.5f };', $start)
if ($end -lt 0) { throw 'Expected fallback path missing' }
$content = $content.Substring(0, $start) + '        ' + $content.Substring($end)
[IO.File]::WriteAllText($target, $content, [Text.UTF8Encoding]::new($false))
