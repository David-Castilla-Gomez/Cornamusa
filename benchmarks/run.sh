#!/usr/bin/env bash
# Cornamusa — runner de benchmarks (bash).
#
# Ejecuta todos los .cor de este directorio con el motor bytecode y
# reporta tiempos. No automatiza comparación; el output es texto plano
# para que se pueda copiar al CHANGELOG o a benchmarks/RESULTS.md.
#
# Uso:
#   ./benchmarks/run.sh                  # usa ./build_v2/cornamusa.exe
#   CORNAMUSA=ruta/a/cornamusa ./benchmarks/run.sh
#
# Para Windows nativo sin bash, usa benchmarks/run.ps1.

set -e

cd "$(dirname "$0")/.."

BIN=${CORNAMUSA:-./build/cornamusa.exe}
if [ ! -x "$BIN" ] && [ ! -f "$BIN" ]; then
    echo "No encuentro el binario en $BIN — compila con 'make build' o pasa CORNAMUSA=..."
    exit 1
fi

echo "Cornamusa benchmarks — bytecode engine"
echo "Binario: $BIN"
echo "Fecha:   $(date 2>/dev/null)"
echo

for f in benchmarks/*.cor; do
    nombre=$(basename "$f" .cor)
    printf "  %-25s " "$nombre"
    # Usamos `time` builtin de bash; redirigimos stdout para que no
    # se mezcle con el reporte. SECONDS solo da segundos enteros, así
    # que usamos la variable interna $EPOCHREALTIME (bash 5+) cuando
    # esté disponible.
    if [ -n "$EPOCHREALTIME" ]; then
        START=$EPOCHREALTIME
        OUT=$("$BIN" --bytecode "$f" 2>&1)
        RC=$?
        END=$EPOCHREALTIME
        # Restar floats sin bc: usamos awk.
        DT=$(awk -v a="$START" -v b="$END" 'BEGIN{printf "%.3fs", b - a}')
    else
        START=$SECONDS
        OUT=$("$BIN" --bytecode "$f" 2>&1)
        RC=$?
        END=$SECONDS
        DT="$((END - START))s"
    fi
    if [ $RC -ne 0 ]; then
        echo "FAIL (rc=$RC)"
        echo "$OUT" | sed 's/^/    /'
    else
        echo "$DT"
    fi
done

echo
echo "Hecho."
