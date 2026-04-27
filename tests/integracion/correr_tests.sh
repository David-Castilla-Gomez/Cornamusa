#!/usr/bin/env bash
# Corre los programas .cor de tests/integracion/ y compara su salida con
# el archivo .esperado correspondiente. Este script se activa en fases
# posteriores; en v0.1.0 el intérprete aún no ejecuta programas, así que
# simplemente sale con éxito si no hay tests definidos.

set -euo pipefail

BINARIO="${1:?uso: $0 <ruta/al/binario/cornamusa>}"
DIR_TESTS="$(dirname "$0")"

shopt -s nullglob
TESTS=("$DIR_TESTS"/*.cor)

if [ ${#TESTS[@]} -eq 0 ]; then
    echo "tests/integracion: sin tests definidos todavía (esperado en v0.1.0)"
    exit 0
fi

fallos=0
for test in "${TESTS[@]}"; do
    nombre=$(basename "$test" .cor)
    esperado="$DIR_TESTS/$nombre.esperado"
    if [ ! -f "$esperado" ]; then
        echo "AVISO: '$test' sin '.esperado' — omitido"
        continue
    fi
    salida=$("$BINARIO" "$test")
    if [ "$salida" != "$(cat "$esperado")" ]; then
        echo "FALLO: $nombre"
        diff <(echo "$salida") "$esperado" || true
        fallos=$((fallos + 1))
    else
        echo "OK:    $nombre"
    fi
done

if [ "$fallos" -gt 0 ]; then
    echo "$fallos test(s) fallaron"
    exit 1
fi
echo "todos los tests de integración pasan"
