# utf8proc — vendoreado en Cornamusa

**Versión:** 2.10.0
**Fuente:** https://github.com/JuliaStrings/utf8proc/releases/tag/v2.10.0
**Licencia:** MIT (compatible con la licencia MIT de Cornamusa). Ver `LICENSE.md`.

## Por qué está vendoreado

Cornamusa requiere normalización Unicode NFC y categorización de code points (decisión [B4](../../decisiones/B4-tildes-y-unicode.md)). Hacerlo a mano implicaría incluir tablas Unicode completas — esencialmente reimplementar utf8proc.

Lo vendoreamos en lugar de usar submódulo o `FetchContent` para que clonar el repo sea suficiente para compilar (sin dependencias de red en build time, sin pasos extra para el usuario).

## Archivos incluidos

| Archivo | Propósito |
|---|---|
| `utf8proc.h` | API pública |
| `utf8proc.c` | Implementación |
| `utf8proc_data.c` | Tablas Unicode generadas |
| `LICENSE.md` | Licencia MIT (obligatoria por la licencia) |
| `README.md` | README original del proyecto |

Todo lo demás (tests, benchmarks, CI, documentación) se omitió por no aportar al binario.

## Funciones que usamos

- `utf8proc_iterate` — decode de un code point UTF-8.
- `utf8proc_category` — categoría Unicode (Letter, Digit, etc.) para identificadores.
- `utf8proc_NFC` — normalización NFC al cargar archivos fuente.

## Actualización

Para actualizar a una versión nueva:

```bash
curl -L https://github.com/JuliaStrings/utf8proc/archive/refs/tags/vX.Y.Z.tar.gz | tar -xz -C /tmp/
cp /tmp/utf8proc-X.Y.Z/{utf8proc.c,utf8proc.h,utf8proc_data.c,LICENSE.md,README.md} vendor/utf8proc/
```

Y actualizar la versión documentada al inicio de este archivo.
