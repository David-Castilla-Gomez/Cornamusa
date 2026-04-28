# libtommath — vendoreado en Cornamusa

**Versión:** 1.3.0
**Fuente:** https://github.com/libtom/libtommath/releases/tag/v1.3.0
**Licencia:** Public Domain / Unlicense (compatible con cualquier licencia, incluida la MIT de Cornamusa). Ver `LICENSE`.

## Por qué está vendoreado

Cornamusa requiere enteros de precisión arbitraria (decisión [B3](../../decisiones/B3-representacion-numerica.md)). Implementar bignum a mano implicaría reinventar lo que libtommath ya hace bien y de forma probada.

## Arquitectura

libtommath 1.3.0 reparte las funciones en ~150 archivos `.c` separados, cada uno con una operación. La librería se compila como `libtommath.a` mediante `sources.cmake` que CMake usa para listar las fuentes.

## Funciones que usamos en Cornamusa

| Operación | Función libtommath |
|---|---|
| Inicializar / liberar | `mp_init`, `mp_clear`, `mp_init_set_int` |
| Asignar entero | `mp_set_l` (long), `mp_set_i64` |
| Aritmética | `mp_add`, `mp_sub`, `mp_mul`, `mp_div`, `mp_mod`, `mp_neg`, `mp_abs` |
| Potencia | `mp_expt_n` (entero ** entero pequeño) |
| Comparación | `mp_cmp`, `mp_cmp_d` |
| A cadena | `mp_to_radix`, `mp_radix_size` |
| De cadena | `mp_read_radix` |

## Actualización

```bash
curl -L https://github.com/libtom/libtommath/archive/refs/tags/vX.Y.Z.tar.gz | tar -xz -C /tmp/
cp /tmp/libtommath-X.Y.Z/{*.c,*.h,sources.cmake,LICENSE,README.md} vendor/libtommath/
```

Y actualizar la versión documentada al inicio de este archivo.
