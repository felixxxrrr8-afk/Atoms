# Картинки для README: скриншоты и анимации на двух языках.
#   powershell -ExecutionPolicy Bypass -File tools\media.ps1                 # всё
#   powershell -ExecutionPolicy Bypass -File tools\media.ps1 -Only nanowire  # один кадр или ролик
#   powershell -ExecutionPolicy Bypass -File tools\media.ps1 -Shots          # только скриншоты
#   powershell -ExecutionPolicy Bypass -File tools\media.ps1 -Clips          # только анимации
# Нужен собранный atoms.exe в корне и Python 3 с Pillow (pip install pillow) для GIF.
# Результат: docs\ru\*.png|gif и docs\en\*.png|gif.
param([string]$Only = '', [switch]$Shots, [switch]$Clips)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
if (-not $Shots -and -not $Clips) { $Shots = $true; $Clips = $true }

# имя, аргументы --shot (сцена, число кадров, ключи)
$shotList = @(
  @('hydrogen-combustion',  '7 450 tab=2'),
  @('nacl-water',           '6 900'),
  @('crystal-melting',      '2 700 tab=1'),
  @('ice',                  '2 500 v5 tab=1'),
  @('cross-section',        '2 150 cut'),
  @('quench-polycrystal',   '9 700'),
  @('glass',                '9 700 v1 color=3'),
  @('gold-nanoparticle',    '11 700'),
  @('nanowire',             '44 1150'),
  @('sintering',            '25 900'),
  @('liquid-vapor',         '40 900 tab=1'),
  @('cavitation',           '43 900'),
  @('adsorption',           '41 700'),
  @('wetting',              '27 900'),
  @('poiseuille-flow',      '29 900'),
  @('shock-tube',           '22 260'),
  @('heat-conduction',      '21 1200 tab=0'),
  @('brownian-motion',      '8 900'),
  @('electrophoresis',      '15 700'),
  @('condensation-graphs',  '4 900 tab=3'),
  @('methane-chlorination', '31 700 tab=2'),
  @('propane-combustion',   '45 500 tab=2'),
  @('ncl3-explosion',       '46 300 tab=2'),
  @('ozone',                '39 700 tab=2'),
  @('nickel-hydrogenation', '33 700 tab=2'),
  @('platinum-catalysis',   '20 700 tab=2'),
  @('acid-ph',              '16 500 tab=2'),
  @('molecule-gallery',     '1 200 nopanel lib=113,112,111,114,110,116,118,117,121,66,63,60,67,64,97,72,95,88'),
  @('library-panel',        '1 60 tab=2 liball scrollto=1400'),
  @('field-objects',        '1 300 demo'),
  @('periodic-table',       '1 30 table hover26'),
  @('scenes-menu',          '1 30 menu'),
  @('settings',             '1 60 settings'),
  @('help',                 '1 60 help')
)
# имя, сцена и ключи, с какого кадра писать, каждый какой кадр, сколько кадров в ролике
$clipList = @(
  @('combustion',  '7 rot',      60,  2, 110),
  @('melting',     '2 rot',       0, 16, 150),
  @('gold',        '11 rot',      0, 10, 125),
  @('nanowire',    '44',          0, 16, 135),
  @('sodium',      '13',          0, 16, 150),
  @('shock-tube',  '22',         40,  2, 110),
  @('ncl3',        '46',         30,  2, 110)
)

function Run([string[]]$argv) {
  $p = Start-Process .\atoms.exe -ArgumentList $argv -PassThru
  if (-not $p.WaitForExit(600000)) { $p.Kill(); throw "atoms.exe завис: $argv" }
}
foreach ($lang in 'ru', 'en') {
  $dir = "docs\$lang"
  New-Item -ItemType Directory -Force $dir | Out-Null
  if ($Shots) {
    foreach ($s in $shotList) {
      if ($Only -and $s[0] -ne $Only) { continue }
      Run (@('--lang', $lang, '--shot') + ($s[1] -split ' ') + @('png', "out=$dir\$($s[0]).png"))
      "$lang $($s[0])"
    }
  }
  if ($Clips) {
    foreach ($c in $clipList) {
      if ($Only -and $c[0] -ne $Only) { continue }
      $tmp = Join-Path $env:TEMP "atoms_clip_$lang`_$($c[0])"
      $parts = @($c[1] -split ' ')
      $extra = if ($parts.Count -gt 1) { $parts[1..($parts.Count - 1)] } else { @() }
      $frames = $c[2] + $c[3] * $c[4] + 2
      for ($try = 1; $try -le 3; $try++) {   # застывший ролик (gif.py вернул 3) переснимается
        if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
        Run (@('--lang', $lang, '--shot', $parts[0], "$frames") + $extra + @('nopanel', 'size=1280x760', 'zoom=0.8', "rec=$tmp", "every=$($c[3])", "from=$($c[2])", 'out=NUL'))
        python tools\gif.py $tmp "$dir\$($c[0]).gif" --width 640 --colors 112
        if ($LASTEXITCODE -ne 3) { break }
      }
      Remove-Item $tmp -Recurse -Force
      "$lang $($c[0]).gif"
    }
  }
}
