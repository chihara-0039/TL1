Add-Type -AssemblyName System.Drawing

$originalPath = 'project/Resources/UI/tutorial/placement_tutorial.png'
$tempPath = 'project/Resources/UI/tutorial/placement_tutorial_temp.png'

$img = [System.Drawing.Image]::FromFile($originalPath)
$bmp = new-object System.Drawing.Bitmap($img)
$img.Dispose() # release the lock on the original file

# 1. Erase the "高低" text
for ($y = 15; $y -lt 90; $y++) {
    for ($x = 295; $x -lt 435; $x++) {
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    }
}

# 2. Draw "上下"
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

$fontName = "Meiryo"
$fontSize = 32
$fontStyle = [System.Drawing.FontStyle]::Bold
$font = new-object System.Drawing.Font($fontName, $fontSize, $fontStyle)

$text = "上下"
$xPos = 308
$yPos = 20

# Draw black outline
$brushBlack = new-object System.Drawing.SolidBrush([System.Drawing.Color]::Black)
for ($dx = -3; $dx -le 3; $dx++) {
    for ($dy = -3; $dy -le 3; $dy++) {
        if ($dx*$dx + $dy*$dy -le 10) {
            $g.DrawString($text, $font, $brushBlack, $xPos + $dx, $yPos + $dy)
        }
    }
}

# Draw white center
$brushWhite = new-object System.Drawing.SolidBrush([System.Drawing.Color]::White)
$g.DrawString($text, $font, $brushWhite, $xPos, $yPos)

$g.Dispose()
$font.Dispose()
$brushBlack.Dispose()
$brushWhite.Dispose()

$bmp.Save($tempPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Move-Item -Path $tempPath -Destination $originalPath -Force
