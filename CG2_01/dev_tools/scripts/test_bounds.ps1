Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile('project/Resources/UI/tutorial/placement_tutorial.png')
$minX = 10000; $maxX = -1; $minY = 10000; $maxY = -1
for ($y = 0; $y -lt 120; $y++) {
    for ($x = 280; $x -lt 450; $x++) {
        $color = $bmp.GetPixel($x, $y)
        if ($color.A -gt 50) {
            if ($x -lt $minX) { $minX = $x }
            if ($x -gt $maxX) { $maxX = $x }
            if ($y -lt $minY) { $minY = $y }
            if ($y -gt $maxY) { $maxY = $y }
        }
    }
}
Write-Output "Bounds: ($minX, $minY) to ($maxX, $maxY)"
$bmp.Dispose()
