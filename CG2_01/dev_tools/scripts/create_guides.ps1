Add-Type -AssemblyName System.Drawing
$font = new-object System.Drawing.Font("Meiryo", 32, [System.Drawing.FontStyle]::Bold)
$brushWhite = new-object System.Drawing.SolidBrush([System.Drawing.Color]::White)
$brushBlack = new-object System.Drawing.SolidBrush([System.Drawing.Color]::Black)

function Create-GuideImage {
    param($text, $filename)
    # 余裕を持たせた大きめの画像を作成
    $bmp = new-object System.Drawing.Bitmap(1000, 100)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    
    # テキストサイズの測定
    $size = $g.MeasureString($text, $font)
    $x = (1000 - $size.Width) / 2
    $y = (100 - $size.Height) / 2
    
    # 縁取り風に描画 (周囲8方向に黒で描画)
    for ($dx = -2; $dx -le 2; $dx++) {
        for ($dy = -2; $dy -le 2; $dy++) {
            if ($dx -ne 0 -or $dy -ne 0) {
                $g.DrawString($text, $font, $brushBlack, $x + $dx, $y + $dy)
            }
        }
    }
    # メインのテキストを白で描画
    $g.DrawString($text, $font, $brushWhite, $x, $y)
    
    $g.Dispose()
    $bmp.Save($filename, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

Create-GuideImage "目的：ゴール（★）を目指せ！" "project/Resources/UI/objective_guide.png"
Create-GuideImage "ステージ選択：WASDキーで選択 / SPACEキーで決定" "project/Resources/UI/stage_select_guide.png"
Create-GuideImage "SPACEキーでタイトルに戻る" "project/Resources/UI/clear_guide.png"

$font.Dispose()
$brushWhite.Dispose()
$brushBlack.Dispose()
