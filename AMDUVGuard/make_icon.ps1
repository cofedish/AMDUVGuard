Add-Type -AssemblyName System.Drawing

function Make-Bmp([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit

    # Background gradient (red -> dark red)
    $rect = New-Object System.Drawing.Rectangle 0, 0, $size, $size
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, ([System.Drawing.Color]::FromArgb(220,40,40)), ([System.Drawing.Color]::FromArgb(140,10,10)), 90.0
    $g.FillRectangle($brush, $rect)
    $brush.Dispose()

    # Inner rounded panel
    $pad = [int]($size * 0.12)
    $inner = New-Object System.Drawing.Rectangle $pad, $pad, ($size-2*$pad), ($size-2*$pad)
    $panel = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(245,245,245))
    $g.FillRectangle($panel, $inner)
    $panel.Dispose()

    # "UV" text
    $fontSize = [Math]::Max(6, [int]($size * 0.45))
    $font = New-Object System.Drawing.Font "Segoe UI", $fontSize, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment = [System.Drawing.StringAlignment]::Center
    $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
    $textBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(180,20,20))
    $rectF = New-Object System.Drawing.RectangleF ([single]$inner.X), ([single]$inner.Y), ([single]$inner.Width), ([single]$inner.Height)
    $g.DrawString("UV", $font, $textBrush, $rectF, $sf)
    $textBrush.Dispose()
    $font.Dispose()
    $sf.Dispose()
    $g.Dispose()
    return $bmp
}

# Build a multi-size .ico file manually so we get crisp icons at every size.
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$entries = @()
$blobs = @()
foreach ($s in $sizes) {
    $bmp = Make-Bmp $s
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $blobs += ,($ms.ToArray())
    $ms.Dispose()
}

$out = New-Object System.IO.MemoryStream
$bw  = New-Object System.IO.BinaryWriter $out
# ICONDIR
$bw.Write([uint16]0)            # reserved
$bw.Write([uint16]1)            # type = icon
$bw.Write([uint16]$sizes.Count) # count
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $blob = $blobs[$i]
    # ICONDIRENTRY
    $bw.Write([byte]($s -band 0xFF))   # width
    $bw.Write([byte]($s -band 0xFF))   # height
    $bw.Write([byte]0)                  # colors
    $bw.Write([byte]0)                  # reserved
    $bw.Write([uint16]1)                # planes
    $bw.Write([uint16]32)               # bpp
    $bw.Write([uint32]$blob.Length)
    $bw.Write([uint32]$offset)
    $offset += $blob.Length
}
foreach ($blob in $blobs) {
    $bw.Write($blob)
}
$bw.Flush()
[System.IO.File]::WriteAllBytes("$PSScriptRoot\src\AMDUVGuard.ico", $out.ToArray())
$bw.Dispose()
$out.Dispose()
Write-Host "Wrote $PSScriptRoot\src\AMDUVGuard.ico"
