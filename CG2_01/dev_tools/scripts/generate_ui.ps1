Add-Type -AssemblyName System.Drawing

function Generate-Text-Image {
    param(
        [string]$text,
        [string]$outPath,
        [int]$width,
        [int]$height,
        [int]$fontSize
    )

    $bmp = new-object System.Drawing.Bitmap($width, $height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    $fontName = "Meiryo"
    $fontStyle = [System.Drawing.FontStyle]::Bold
    $font = new-object System.Drawing.Font($fontName, $fontSize, $fontStyle)

    # Measure text to center it
    $size = $g.MeasureString($text, $font)
    $xPos = ($width - $size.Width) / 2
    $yPos = ($height - $size.Height) / 2

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

    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

Generate-Text-Image -text "目的：ゴール（★）を目指せ！" -outPath "project/Resources/UI/objective_guide.png" -width 600 -height 80 -fontSize 28
Generate-Text-Image -text "WASDキーで選択 / SPACEキーで決定" -outPath "project/Resources/UI/stage_select_guide.png" -width 800 -height 80 -fontSize 32
Generate-Text-Image -text "SPACEキーで戻る" -outPath "project/Resources/UI/clear_guide.png" -width 600 -height 80 -fontSize 32

