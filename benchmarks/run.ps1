# Cornamusa - runner de benchmarks (PowerShell).
#
# Ejecuta todos los .cor de este directorio con el motor bytecode y
# reporta tiempos. No automatiza comparacion; el output es texto plano
# para que se pueda copiar al CHANGELOG o a benchmarks/RESULTS.md.
#
# Uso:
#   ./benchmarks/run.ps1
#   $env:CORNAMUSA = 'ruta\a\cornamusa.exe'; ./benchmarks/run.ps1

param(
    [string]$CornamusaBin = $env:CORNAMUSA
)

if (-not $CornamusaBin) {
    $CornamusaBin = './build/cornamusa.exe'
}

if (-not (Test-Path $CornamusaBin)) {
    Write-Host "No encuentro el binario en $CornamusaBin"
    Write-Host "Compila con 'make build' o pasa -CornamusaBin <ruta>."
    exit 1
}

# Vamos a la raiz del repo (parent del directorio de este script).
$root = Split-Path (Split-Path $MyInvocation.MyCommand.Path)
Set-Location $root

Write-Host "Cornamusa benchmarks - bytecode engine"
Write-Host "Binario: $CornamusaBin"
Write-Host "Fecha:   $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host ""

$benchmarks = Get-ChildItem 'benchmarks/*.cor' | Sort-Object Name
foreach ($f in $benchmarks) {
    $nombre = $f.BaseName
    Write-Host -NoNewline ("  {0,-25} " -f $nombre)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $out = & $CornamusaBin '--bytecode' $f.FullName 2>&1
    $sw.Stop()
    $rc = $LASTEXITCODE
    $secs = '{0:N3}s' -f $sw.Elapsed.TotalSeconds
    if ($rc -ne 0) {
        Write-Host "FAIL (rc=$rc)"
        $out | ForEach-Object { Write-Host "    $_" }
    } else {
        Write-Host $secs
    }
}

Write-Host ""
Write-Host "Hecho."
