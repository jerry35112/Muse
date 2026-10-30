$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$frames = [System.Collections.Generic.List[byte[]]]::new()
$sizes = @(16,32,48,256)
foreach ($size in $sizes) {
    $bitmap = [System.Drawing.Bitmap]::new($size, $size)
    $g = [System.Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.ScaleTransform($size/256.0, $size/256.0)
    $background = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $background.AddArc(0,0,120,120,180,90); $background.AddArc(136,0,120,120,270,90)
    $background.AddArc(136,136,120,120,0,90); $background.AddArc(0,136,120,120,90,90); $background.CloseFigure()
    $dark = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#12171d'))
    $mint = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#7ad4b3'))
    $teal = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#458d83'))
    $g.FillPath($dark,$background)
    $points = [System.Drawing.PointF[]]@([System.Drawing.PointF]::new(48,176),[System.Drawing.PointF]::new(82,68),[System.Drawing.PointF]::new(93,73),[System.Drawing.PointF]::new(128,115),[System.Drawing.PointF]::new(163,73),[System.Drawing.PointF]::new(174,68),[System.Drawing.PointF]::new(208,176),[System.Drawing.PointF]::new(178,176),[System.Drawing.PointF]::new(158,110),[System.Drawing.PointF]::new(128,147),[System.Drawing.PointF]::new(98,110),[System.Drawing.PointF]::new(78,176))
    $g.FillPolygon($mint,$points)
    $g.FillPolygon($teal,[System.Drawing.PointF[]]@([System.Drawing.PointF]::new(108,177),[System.Drawing.PointF]::new(128,154),[System.Drawing.PointF]::new(148,177)))
    $stream = [System.IO.MemoryStream]::new(); $bitmap.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
    $frames.Add($stream.ToArray())
    if ($size -eq 256) { $bitmap.Save((Join-Path $root 'assets/muse.png'),[System.Drawing.Imaging.ImageFormat]::Png) }
    $stream.Dispose(); $g.Dispose(); $bitmap.Dispose(); $background.Dispose(); $dark.Dispose(); $mint.Dispose(); $teal.Dispose()
}
$file = [System.IO.File]::Create((Join-Path $root 'assets/muse.ico'))
$writer = [System.IO.BinaryWriter]::new($file)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
$offset = 6 + 16*$sizes.Count
for ($i=0; $i -lt $sizes.Count; $i++) {
    $dimension = if ($sizes[$i] -eq 256) {0} else {$sizes[$i]}
    $writer.Write([byte]$dimension); $writer.Write([byte]$dimension); $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$frames[$i].Length); $writer.Write([uint32]$offset)
    $offset += $frames[$i].Length
}
foreach ($frame in $frames) { $writer.Write($frame) }
$writer.Dispose(); $file.Dispose()
