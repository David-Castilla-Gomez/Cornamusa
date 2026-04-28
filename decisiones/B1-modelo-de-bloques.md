# B1 — Modelo de delimitación de bloques

**Estado:** ✅ **Decidido**.
**Fecha de propuesta:** 2026-04-28
**Fecha de decisión:** 2026-04-28
**Decisor:** David Castilla
**Bloqueador identificado en:** [REPASO_CRITICO.md](../REPASO_CRITICO.md), problema B1.

**Decisión:** Opción **B' (B+)** — apertura con `:`, cierre con `fin <etiqueta>`. Indentación es decoración estilística, no semántica.

## Contexto

Cornamusa necesita un mecanismo sintáctico para delimitar bloques anidados (cuerpos de funciones, clases, condicionales, bucles, etc.). La elección define la arquitectura del lexer y el parser, y es prácticamente irreversible una vez escrita una línea de código real.

El ESPEC borrador propone **indentación significativa** estilo Python. El repaso crítico cuestionó si es la mejor opción dado:
- Lenguaje host: C (sin librerías de soporte).
- Modelo de referencia: clox usa llaves precisamente por simplicidad.
- Filosofía: castellano natural y pedagógico.
- Política del proyecto: paciencia, calidad, no prisa.

Esta decisión no es solo técnica, es también de **identidad del lenguaje**. Define cómo se ve un programa Cornamusa "típico".

## Programa de referencia

Para comparar las opciones usamos un fragmento real del ejemplo `07_clases_herencia.cor`:

> Una clase `Animal` con método abstracto, una subclase `Perro`, y un bucle que llama a un método polimórfico imprimiendo el resultado bajo una condición.

---

## Opción A — Llaves estilo C

```cornamusa
clase Animal {
    funcion __iniciar__(yo, nombre, edad) {
        yo.nombre = nombre
        yo.edad = edad
    }

    funcion hablar(yo) {
        lanzar ErrorRuntime("método abstracto")
    }
}

clase Perro extiende Animal {
    funcion hablar(yo) {
        retornar f"{yo.nombre} dice ¡guau!"
    }
}

para animal en animales {
    si animal.edad > 0 {
        imprimir(animal.hablar())
    }
}
```

**Pros**
- Lexer y parser **mínimos** — clox al 95% reutilizable.
- Robusto frente a copy-paste, refactor automático, formatters.
- Cualquier editor del mundo conoce `{` `}`.
- Strings multilínea, listas anidadas, lambdas — todo trivial.
- Errores claros: `error: '}' esperado al cerrar el bloque iniciado en línea 12`.

**Contras**
- **Pierde el sabor castellano-Python** que motiva el lenguaje.
- En castellano la llave no significa nada (en inglés tampoco, pero allí ya hay tradición).
- Visualmente "ruidoso" en programas pequeños — es C, no es nuestro objetivo pedagógico.
- Tema cultural: programadores hispanohablantes asocian llaves con C/Java/JS, no con un lenguaje "para principiantes".

**Coste de implementación:** ~1 semana para lexer+parser bien hechos.

---

## Opción B — Bloques con `fin` (etiquetado)

```cornamusa
clase Animal
    funcion __iniciar__(yo, nombre, edad)
        yo.nombre = nombre
        yo.edad = edad
    fin funcion

    funcion hablar(yo)
        lanzar ErrorRuntime("método abstracto")
    fin funcion
fin clase

clase Perro extiende Animal
    funcion hablar(yo)
        retornar f"{yo.nombre} dice ¡guau!"
    fin funcion
fin clase

para animal en animales
    si animal.edad > 0
        imprimir(animal.hablar())
    fin si
fin para
```

**Pros**
- Lexer y parser **simples** (similar a Lua / Pascal / Ada).
- Tradición fuerte en castellano: pseudocódigo educativo, [Latino](https://github.com/primitivorm/latino), libros de texto españoles.
- **Errores excelentes**: `error: 'fin si' esperado en línea 27, pero encontrado 'fin para'`. El compilador sabe qué bloque se está cerrando.
- No depende de espaciado: tolerante a indentación rota.
- Indentación es decoración, no semántica → pegar código de cualquier sitio funciona.
- Sin keyword apertura `:` (pero podemos añadirlo opcionalmente, ver variante B').

**Contras**
- **Verboso**: programas más largos verticalmente.
- Anidación profunda produce filas de `fin fin fin fin`.
- Estéticamente distante de Python (reduce el "puente" pedagógico que el ESPEC pretende).
- Cultural: muchos programadores asocian `fin` con BASIC/Pascal, lenguajes "antiguos".

**Variante B' — `fin` con `:` de apertura (compromiso visual)**

```cornamusa
clase Animal:
    funcion __iniciar__(yo, nombre, edad):
        yo.nombre = nombre
    fin funcion
fin clase
```

Mantiene el `:` de Python como pista visual de "abro bloque" sin requerir indentación significativa. El cierre explícito permite pegar mal indentado sin romper.

**Coste de implementación:** ~1 semana para lexer+parser. El `fin` etiquetado añade tabla de keywords compuestas (`fin clase`, `fin funcion`, `fin si`, `fin para`, `fin mientras`, `fin intentar`).

---

## Opción C — Indentación significativa (estilo Python, ESPEC actual)

```cornamusa
clase Animal:
    funcion __iniciar__(yo, nombre, edad):
        yo.nombre = nombre
        yo.edad = edad

    funcion hablar(yo):
        lanzar ErrorRuntime("método abstracto")


clase Perro extiende Animal:
    funcion hablar(yo):
        retornar f"{yo.nombre} dice ¡guau!"


para animal en animales:
    si animal.edad > 0:
        imprimir(animal.hablar())
```

**Pros**
- **Estéticamente impecable**, idéntica a Python.
- Programas cortos y legibles desde el primer minuto.
- Fuerza buen estilo: indentación correcta = código correcto.
- Cero ruido sintáctico — solo lo esencial.
- **Es el modelo del ESPEC actual y de los 12 ejemplos ya escritos.**

**Contras**
- **Lexer 5-10x más complejo** que las opciones A/B:
  - Pila de niveles de indentación (`INDENT`/`DEDENT` virtuales).
  - Continuación dentro de `()`, `[]`, `{}` desactiva indentación.
  - Tabs vs espacios: prohibir uno, normalizar el otro.
  - Comentarios y líneas vacías dentro de bloques.
  - Strings multilínea no afectan indentación.
- **Errores opacos**: `IndentationError: expected an indented block` no dice qué bloque ni dónde empezó.
- Frágil al copy-paste entre editores con configuraciones distintas.
- Diff/merge en git produce conflictos sutiles cuando alguien re-indenta.
- Imposible escribir one-liners (`si x: hacer()` solo en una línea — sí es posible, pero requiere caso especial).

**Coste de implementación:** ~3-4 semanas para hacerlo bien. El lexer de CPython (`Parser/tokenizer.c`) tiene ~3000 líneas dedicadas, y aún así produce regresiones cada año. Probable: 500-1000 líneas de C bien probadas.

**Riesgo:** mucha gente subestima esta opción. Es la fuente #1 de bugs y de issues en lenguajes que la adoptan.

---

## Opción D — Llaves opcionales + indentación semántica (Scala 3, Haskell)

```cornamusa
clase Animal:
    funcion __iniciar__(yo, nombre, edad):
        yo.nombre = nombre
        yo.edad = edad
```

O equivalentemente:

```cornamusa
clase Animal { funcion __iniciar__(yo, nombre, edad) { yo.nombre = nombre } }
```

**Pros**
- Flexibilidad: el usuario elige.
- Permite one-liners cuando convienen.

**Contras**
- **Dos formas de escribir lo mismo** rompe "una sola manera obvia".
- Implementación = unión de A + C, **el doble de complejidad**.
- En revisiones de código: discusiones eternas sobre qué estilo usar.
- En la práctica los proyectos terminan adoptando una sola forma → la otra es deuda técnica viva.

**Coste:** ~5-6 semanas. Descartable salvo razón fuerte.

---

## Comparativa resumida

| Criterio | A (llaves) | B (`fin`) | C (indent) | D (mixto) |
|---|---|---|---|---|
| Coste implementación | 🟢 Bajo | 🟢 Bajo | 🔴 Alto | 🔴 Muy alto |
| Calidad de errores | 🟢 Excelente | 🟢 Excelente | 🟡 Mala | 🟡 Variable |
| Sabor castellano | 🔴 Nulo | 🟢 Fuerte | 🟡 Medio (Python-ish) | 🟡 Medio |
| Concisión visual | 🟡 Media | 🔴 Verboso | 🟢 Excelente | 🟢 Excelente |
| Robustez (copy-paste, diff) | 🟢 Alta | 🟢 Alta | 🔴 Baja | 🟡 Media |
| Familiar a principiantes hispano | 🟡 De C/Java | 🟢 De pseudocódigo | 🟡 De Python | 🟡 Confuso |
| Encaja con clox de referencia | 🟢 Sí | 🟡 Adaptable | 🔴 Reescribir | 🔴 Reescribir |
| Filosofía "una sola forma" | 🟢 Sí | 🟢 Sí | 🟢 Sí | 🔴 No |

---

## Análisis adicional: ¿qué hacen otros lenguajes "en español"?

| Lenguaje | Año | Modelo de bloques |
|---|---|---|
| [Latino](https://github.com/primitivorm/latino) | 2014 | `inicio` … `fin` |
| [Lexico](https://es.wikipedia.org/wiki/Lexico) | 2001 | `inicio` … `fin` |
| RoboMind (esp.) | 2005 | `inicio` … `fin` |
| Pseudocódigo PSeInt | 2003 | `Inicio` … `Fin`, `FinSi`, `FinMientras` |

**Patrón observado:** todos los lenguajes educativos en castellano usan **palabras de cierre explícitas**, no indentación ni llaves. Hay tradición de varios miles de programadores formados con esta convención.

---

## Análisis adicional: ¿para quién es Cornamusa?

Tres personas hipotéticas:

1. **Ana, 14 años, primer lenguaje.** Aprende en clase de informática. Le cuesta el inglés, le ayuda que `funcion`, `si`, `mientras` se llamen así. Sus errores típicos: olvidar el `:`, indentar mal, no cerrar el paréntesis.
2. **Luis, 35 años, ingeniero industrial sin background CS.** Quiere automatizar tareas. Conoce Excel y un poco de VBA. Cualquier error críptico le frustra.
3. **María, 22 años, estudiante de CS.** Conoce Python. Cornamusa es curiosidad o herramienta para enseñar a su hermano pequeño.

Para Ana: **errores claros** > concisión.
Para Luis: **errores claros** y **tolerancia a copia-pega** > estética.
Para María: **familiaridad con Python** > todo lo demás.

Las opciones B y C empatan en peso a Ana+Luis vs María. Pero los errores de la opción C son notoriamente malos, lo que penaliza a Ana+Luis.

---

## Recomendación

**Opción B' (variante de B con `:` opcional al abrir)**.

Sintaxis: `clase X:` abre bloque, `fin clase` lo cierra. Indentación es estilística, no semántica.

```cornamusa
clase Animal:
    funcion __iniciar__(yo, nombre, edad):
        yo.nombre = nombre
        yo.edad = edad
    fin funcion
fin clase
```

**Justificación:**
1. **Mejores errores posibles** — el parser sabe siempre qué se está cerrando, gracias a la etiqueta.
2. **Tradición castellana**: alineado con Latino/Lexico/PSeInt y miles de programadores formados así.
3. **Implementación trivial** — extiende clox con tabla de cierres compuestos. ~1 semana.
4. **El `:` mantiene el puente visual con Python** sin pagar el coste de la indentación significativa.
5. **Robusto a refactors** — copy-paste de bloques no produce bugs por indentación.
6. **No prematuro**: si en v0.5 descubrimos que `fin` se siente verboso y queremos migrar a indentación, el lexer puede aprenderlo gradualmente. Migrar de indentación a `fin` es más fácil que al revés.

**Trade-off aceptado:** verbosidad. Es el precio por errores claros y por una identidad castellana auténtica que no copia mecánicamente a Python.

**Anti-pattern explícito:** ningún `fin` desnudo. Siempre `fin <lo-que-cierra>`. Es 5 caracteres más, mil veces mejores errores.

---

## Decisión

**Opción B' (B+) adoptada el 2026-04-28.**

Apertura de bloque con `:` (mantiene el puente visual con Python), cierre con `fin <etiqueta>` (mantiene la tradición castellana de PSeInt/Latino y produce errores de parser excelentes). Indentación es estilística, no semántica.

### Reglas detalladas (resoluciones que el ADR no cerraba)

1. **Etiquetas de cierre permitidas:**
   - `fin si` — cierra `si` / `sino si` / `sino`.
   - `fin mientras` — cierra `mientras`.
   - `fin para` — cierra `para`.
   - `fin funcion` — cierra `funcion`/`función`.
   - `fin clase` — cierra `clase`.
   - `fin intentar` — cierra `intentar` / `atrapar` / `finalmente`.

2. **Cláusulas continuadoras** (`sino si`, `sino`, `atrapar`, `finalmente`) **no son bloques independientes**. Comparten el `fin` del bloque que las contiene:

   ```cornamusa
   si x > 0:
       imprimir("positivo")
   sino si x < 0:
       imprimir("negativo")
   sino:
       imprimir("cero")
   fin si
   ```

3. **One-liners**: si tras `:` viene token no-newline, la sentencia ocupa una sola línea y **no requiere `fin`**:

   ```cornamusa
   si x > 0: imprimir(x)         # válido
   ```

   Si tras `:` viene newline, es bloque multilínea y **exige** `fin <etiqueta>`.

4. **Bloque vacío**: se rellena con `pasar` (estilo Python). El `fin <etiqueta>` sigue siendo obligatorio:

   ```cornamusa
   funcion no_hace_nada():
       pasar
   fin funcion
   ```

5. **Anti-pattern explícito**: `fin` desnudo (sin etiqueta) es **error de sintaxis**. La etiqueta es parte del lenguaje, no opcional.

6. **Tildes en etiquetas**: la regla general de tildes opcionales (B4, pendiente) aplicará también aquí. Provisionalmente: la forma canónica es **sin tilde** (`fin funcion`), la forma con tilde (`fin función`) se acepta cuando se resuelva B4.

7. **Indentación recomendada**: 4 espacios. **No semántica**: el lexer la ignora. Editores y formatters pueden imponerla por estilo, pero el lenguaje no.

---

## Consecuencias (cuando se decida)

Si **B/B'**:
- Reescribir los 12 ejemplos en `examples/` con `fin <X>`.
- Actualizar ESPEC.md sección 5 (gramática) y sección 7 (programa de ejemplo).
- Lexer (Fase 2) emite tokens compuestos para `fin clase`, `fin funcion`, etc., o tokens simples y el parser los une.
- Tabla de keywords reservadas: añadir `fin`.

Si **A**:
- Reescribir ejemplos con `{ }`.
- Actualizar ESPEC: eliminar `:` como apertura, eliminar mención de indentación significativa.
- Lexer trivial.

Si **C**:
- Mantener ESPEC y ejemplos actuales.
- **Reservar +3 semanas** para Fase 2 (lexer con INDENT/DEDENT correcto).
- Estudiar el tokenizador de CPython como referencia.

Si **D**: descartada salvo razón fuerte.
