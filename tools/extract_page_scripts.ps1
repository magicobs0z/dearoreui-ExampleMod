# One-shot extraction of inline page scripts (R"js(...)js") into independent
# .js assets for the T1 asset-ization. Dev helper; not part of the build.
$ErrorActionPreference = 'Stop'

function Extract-Raw {
    param([string]$Src, [string]$Out)
    $text = [System.IO.File]::ReadAllText($Src)
    $open = 'R"js('
    $close = ')js"'
    $start = $text.IndexOf($open)
    if ($start -lt 0) { throw "open delimiter not found in $Src" }
    $bodyStart = $start + $open.Length
    $end = $text.IndexOf($close, $bodyStart)
    if ($end -lt 0) { throw "close delimiter not found in $Src" }
    $body = $text.Substring($bodyStart, $end - $bodyStart)
    $dir = [System.IO.Path]::GetDirectoryName($Out)
    if (![System.IO.Directory]::Exists($dir)) { [System.IO.Directory]::CreateDirectory($dir) | Out-Null }
    [System.IO.File]::WriteAllText($Out, $body, [System.Text.UTF8Encoding]::new($false))
    Write-Host "extracted $Out ($($body.Length) chars)"
}

Extract-Raw 'src\mod\examples\ex03_page_script.h' 'assets\scripts\ex03_page_script.js'
Extract-Raw 'src\mod\examples\ex04_events.h'       'assets\scripts\ex04_events.js'
Extract-Raw 'src\mod\examples\ex05_frame_data.h'   'assets\scripts\ex05_frame_data.js'
Extract-Raw 'src\mod\examples\ex06_host_method.h'  'assets\scripts\ex06_host_method.js'
Extract-Raw 'src\mod\examples\ex07_calendar.cpp'   'assets\scripts\ex07_calendar.js'
