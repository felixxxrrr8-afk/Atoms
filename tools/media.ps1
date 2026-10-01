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
  @('ice',                  '2 40 v5 tab=1 zoom=0.6 notoast'),
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
  @('molecule-gallery',     '1 200 nopanel lib=135,134,133,136,132,138,140,139,143,84,81,78,85,82,115,90,113,106'),
  @('valence-gallery',      '1 200 nopanel cell=4.2 zoom=0.8 lib=48,55,57,62,58,54,59,61,63,50,126,127'),
  @('library-panel',        '1 60 tab=2 liball scrollto=1400'),
  @('field-objects',        '1 300 demo'),
  @('scene-editor',         '1 150 tab=5 faces=012340'),
  @('periodic-table',       '1 30 table hover26'),
  @('orbitals-gallery',     '1 200 nopanel orbitals cell=2.4 cols=3 zoom=0.72 lib=0,4,93,71,16,2'),
  @('orbitals-flame',       '14 260 nopanel orbitals zoom=0.5'),
  @('atom-cloud',           '1 40 atomview avz=54 avmode=0'),
  @('orbital-shapes',       '1 30 atomview avz=6 avmode=2'),
  @('orbital-cloud',        '1 40 atomview avz=26 avmode=1 avn=3 avl=2 avm=1'),
  @('orbital-2p',           '1 40 atomview avz=6 avmode=1 avn=2 avl=1 avm=2'),
  @('reactor',              '103 400'),
  @('proton',               '110 330'),
  @('led',                  '121 3000'),
  @('mosfet',               '122 3000'),
  @('double-slit',          '131 1500'),
  @('tunneling',            '130 330'),
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
  @('ncl3',        '46',         30,  2, 110),
  @('double-slit', '131',         0,  6, 120),
  @('tunneling',   '130',         0,  3, 110),
  @('proton',      '110',         0,  3, 110),
  @('nuclear-explosion', '104',   0,  3, 110)
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
