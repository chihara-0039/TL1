Add-Type -AssemblyName System.Drawing
$font = new-object System.Drawing.Font("Meiryo", 28, [System.Drawing.FontStyle]::Bold)
$brushWhite = new-object System.Drawing.SolidBrush([System.Drawing.Color]::White)
$brushBlack = new-object System.Drawing.SolidBrush([System.Drawing.Color]::Black)

function Create-FitGuideImage {
    param($text, $filename)
    # 一旦ダミーでサイズ測定
    $bmpDummy = new-object System.Drawing.Bitmap(1, 1)
    $gDummy = [System.Drawing.Graphics]::FromImage($bmpDummy)
    $size = $gDummy.MeasureString($text, $font)
    $gDummy.Dispose()
    $bmpDummy.Dispose()

    $width = [int]$size.Width + 20
    $height = [int]$size.Height + 20

    $bmp = new-object System.Drawing.Bitmap($width, $height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    
    $x = 10
    $y = 10
    
    for ($dx = -2; $dx -le 2; $dx++) {
        for ($dy = -2; $dy -le 2; $dy++) {
            if ($dx -ne 0 -or $dy -ne 0) {
                $g.DrawString($text, $font, $brushBlack, $x + $dx, $y + $dy)
            }
        }
    }
    $g.DrawString($text, $font, $brushWhite, $x, $y)
    
    $g.Dispose()
    $bmp.Save($filename, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    
    Write-Output "$filename : $width x $height"
}

Create-FitGuideImage "目的：ゴール（★）を目指せ！" "project/Resources/UI/objective_guide.png"
Create-FitGuideImage "ステージ選択：WASDキーで選択 / SPACEキーで決定" "project/Resources/UI/stage_select_guide.png"
Create-FitGuideImage "SPACEキーで最初の画面に戻る" "project/Resources/UI/clear_guide.png"

$font.Dispose()
$brushWhite.Dispose()
$brushBlack.Dispose()
