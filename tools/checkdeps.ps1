# Проверка пакета программы: каждый exe и dll в папке должен зависеть только от библиотек самой Windows или от лежащих
# рядом. Среды выполнения Visual C++ (vcruntime, msvcp, vcomp, concrt…) на чистой Windows нет: если какой-то файл пакета
# её требует, а рядом её нет, программа без Visual C++ Redistributable не запустится. Нужен dumpbin (среда vcvars).
#   powershell -ExecutionPolicy Bypass -File tools\checkdeps.ps1 .build\setup
# Сообщения — по-английски, как у build.bat: их читают в журнале сборки CI.
param([Parameter(Mandatory = $true)][string]$Dir)
$vcRuntime = '^(vcruntime|msvcp|vcomp|concrt|vccorlib|ucrtbase|mfc|atl)'
$bad = 0
foreach ($f in Get-ChildItem -Path (Join-Path $Dir '*') -Include *.exe, *.dll -File) {
    $deps = @(& dumpbin /nologo /dependents $f.FullName | ForEach-Object { if ($_ -match '^\s+(\S+\.dll)\s*$') { $Matches[1] } })
    foreach ($d in $deps) {
        if ($d -match '^(ucrtbased|vcruntime\d+d|msvcp\d+d|vcomp\d+d)\.dll$') { Write-Host "$($f.Name): debug runtime $d"; $bad++; continue }
        if ($d -match '^ucrtbase\.dll$') { continue }   # универсальная CRT — часть Windows 10 и 11
        if ($d -match $vcRuntime -and -not (Test-Path (Join-Path $Dir $d))) { Write-Host "$($f.Name): needs $d, which is not in the package"; $bad++ }
    }
    Write-Host ('{0,-20} {1}' -f $f.Name, ($deps -join ' '))
}
if ($bad) { Write-Host "the package will not start on a clean Windows: $bad problem(s)"; exit 1 }
Write-Host 'dependencies: OK (only Windows DLLs and the DLLs in the package)'
