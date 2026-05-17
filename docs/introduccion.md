# Cornamusa

> Un lenguaje de programación dinámico, interpretado y **enteramente en castellano**.

Cornamusa es un lenguaje pensado para que aprender a programar no requiera dominar el inglés primero. Las palabras clave, los built-ins y los mensajes de error están en castellano natural; los identificadores admiten Unicode (incluyendo `ñ` y tildes).

```cornamusa
clase Persona:
    funcion __iniciar__(yo, nombre, edad):
        yo.nombre = nombre
        yo.edad = edad
    fin funcion

    funcion saludar(yo):
        retornar "Hola, soy " + yo.nombre
    fin funcion
fin clase

importar matematicas

para persona en [Persona("Ana", 30), Persona("Luis", 25)]:
    imprimir(persona.saludar())
fin para

imprimir("PI =", matematicas.PI)
imprimir("100! =", matematicas.factorial(100))
```

## Si vienes a aprender

→ **[Tutorial paso a paso](tutorial.html)**

Pensado para alguien que nunca ha programado o que viene de otro lenguaje. Cubre desde "hola mundo" hasta clases, módulos y manejo de excepciones, con código ejecutable validado contra el intérprete.

## Si necesitas consultar algo concreto

→ **[Referencia rápida](referencia.html)**

Cheatsheet con tablas densas: sintaxis, operadores, built-ins, stdlib, errores comunes. Para abrir cuando estás escribiendo y olvidaste cómo se llamaba algo.

## Si quieres entender el diseño

→ **[Especificación formal](https://github.com/David-Castilla-Gomez/Cornamusa/blob/main/ESPEC.md)** + **[Decisiones (ADRs)](https://github.com/David-Castilla-Gomez/Cornamusa/tree/main/decisiones)**

ESPEC.md tiene la gramática EBNF, semántica formal y tipos. Las ADRs (`B1` a `B10`) razonan cada decisión de diseño grande del proyecto.

## Estado del proyecto

Cornamusa es **estable** y maduro. Lenguaje completo con paridad sintáctica cercana a Python 3.10+: OOP con herencia y dunders, closures con `nolocal`, pattern matching, generadores, comprehensions, destructuring, `*args`/`**kwargs`, context managers. GC mark-sweep, excepciones con traceback multi-frame, diecisiete módulos de stdlib. Toda la documentación está validada contra el intérprete real.

| Hito | Versión | Estado |
|---|---|---|
| VM bytecode + closures + excepciones | v0.6 | ✅ |
| Clases, herencia, GC, módulos | v0.7–v0.9 | ✅ |
| Inline caching + small-int tagging | v0.10–v0.11 | ✅ |
| Dunders, `nolocal`, context managers | v1.2–v1.13 | ✅ |
| Pattern matching (`coincidir`) | v1.15–v1.16 | ✅ |
| Stdlib amplia (alcanza 17 módulos) | v1.8–v1.73 | ✅ |
| Destructuring, `*args`/`**kwargs`, spread | v1.21–v1.25 | ✅ |
| Comprehensions y generadores | v1.30–v1.34 | ✅ |
| Errores con sugerencias + traceback | v1.35–v1.38 | ✅ |
| Performance: `-O3` + LTO | v1.40 | ✅ |
| Dunders de coerción `__repr__` y `__booleano__` | v1.41 | ✅ |
| `__hash__` + `__igual__` — instancias hashables por valor | v1.42 | ✅ |
| `__siguiente__` — iteradores lazy stateful | v1.43 | ✅ |
| Ternaria + slicing assignment | v1.44 | ✅ |
| F-string format specifiers | v1.45 | ✅ |
| Multi-recurso `con` + combinar `*args`/`**kwargs` (cierre Fase 4) | v1.46 | ✅ |
| REPL con history y line editing (abre Fase 5: tooling) | v1.47 | ✅ |
| Formateador integrado `cornamusa fmt` (`--check`, `--stdout`, idempotente) | v1.48 | ✅ |
| Linter integrado `cornamusa lint` (codigo inalcanzable, `pasar` redundante, `== nulo`, imports no usados) | v1.49 | ✅ |
| Scope analysis en el linter (`unused-local`, `unused-param` con respeto a closures/`nolocal`) | v1.50 | ✅ |
| Generador de documentacion `cornamusa docs` (Markdown desde firmas + comentarios) | v1.51 | ✅ |
| LSP server `cornamusa lsp` (JSON-RPC por stdio, diagnostics en tiempo real al editor) | v1.52 | ✅ |
| LSP polish: parse errors detallados + `textDocument/hover` para funciones y clases | v1.53 | ✅ |
| LSP: `textDocument/definition` (goto-def) + `textDocument/formatting` (delega al formateador) | v1.54 | ✅ |
| Linter: `shadow`, `unused-loop-var`, `mutable-default` (9 categorias en total) | v1.55 | ✅ |
| `borrar d[k]` / `borrar obj.attr` + aug-assign sobre atributos (`obj.x += 1`) | v1.56 | ✅ |
| `global X` implementado en bytecode VM (asignaciones a scope de módulo desde función) | v1.57 | ✅ |
| Stdlib `csv` (parser/writer RFC 4180-like, helpers de archivo) | v1.58 | ✅ |
| Stdlib `base64` (codec RFC 4648 en C nativo) | v1.59 | ✅ |
| Stdlib `hashing` (SHA-256 FIPS 180-4 + MD5 RFC 1321, ambos nativos en C) | v1.60 | ✅ |
| Perf round 2: `cadena_unir` nativo + csv con iterator (33× speedup csv_parse_1000) | v1.61 | ✅ |
| Perf round 2 cont.: 5 nativas más para `cadenas` tras audit (indice_de, empieza_con, ...) | v1.62 | ✅ |
| Linter `concat-in-loop` (10ª categoría) — detecta `x = x + cadena` dentro de bucles | v1.63 | ✅ |
| Linter `# noqa: <categoria>` directive — supresión selectiva por línea | v1.64 | ✅ |
| HMAC-SHA-256 + HMAC-MD5 nativos (RFC 2104/4231) en `hashing` | v1.65 | ✅ |
| base64 URL-safe (RFC 4648 §5, `-_` sin padding, prerequisito JWT) | v1.66 | ✅ |
| Stdlib `jwt` (RFC 7519 HS256) pure-Cornamusa sobre json+base64+hashing | v1.67 | ✅ |
| Linter `same-comparison` (11ª categoría) — detecta `x == x`, `x < x`, typos clásicos | v1.68 | ✅ |
| Linter `empty-except` (12ª categoría) — `atrapar X: pasar` silencia errores | v1.69 | ✅ |
| `jwt.expirado()` + `jwt.decodificar_y_validar()` — validación de claims `exp`/`nbf` | v1.70 | ✅ |
| Profiler determinista `cornamusa prof` (tabla por función: llamadas/total/self/per-call) | v1.71 | ✅ |
| Decoradores `@nombre` (stacking, factories `@retry(3)`, funciones anidadas) | v1.72 | ✅ |
| Stdlib `tiempo` — `monotonic()`, `dormir(s)`, `Cronometro`, `epoch_ms()` (17º módulo) | v1.73 | ✅ |
| Auditoría: tests JWT seguridad, csv unit tests, fix `d[k]` atrapable | v1.74 | ✅ |
| Coverage tracker `cornamusa cov` (% líneas top-level cubiertas) | v1.75 | ✅ |
| Debugger interactivo `cornamusa depurar` (breakpoints + step + inspect) | v1.76 | ✅ |
| Decoradores `@x` sobre métodos de clase (con stacking) | v1.77 | ✅ |
| `@propiedad` — getters invocados al acceder al atributo | v1.78 | ✅ |
| Tutorial expandido a curso (~35 ejercicios + soluciones validadas) | v1.79 | ✅ |
| Limpieza stdlib (dead code eliminado, params renombrados) | v1.80 | ✅ |

## Probar Cornamusa en 5 minutos

```bash
git clone https://github.com/David-Castilla-Gomez/Cornamusa.git
cd Cornamusa
cmake -B build && cmake --build build
./build/cornamusa --bytecode examples/13_factorial_jugable.cor
```

## Licencia

[MIT](https://github.com/David-Castilla-Gomez/Cornamusa/blob/main/LICENSE) — libre para uso personal, educativo y comercial.

---

*Cornamusa* — del castellano antiguo, **gaita** o instrumento de viento.
