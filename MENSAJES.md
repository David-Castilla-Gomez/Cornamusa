# Estándar de calidad de mensajes de error

**Versión:** 1.0 (2026-04-28)
**Estado:** Documento normativo. Todo error producido por Cornamusa debe cumplir este estándar.

---

## 1. Filosofía

Los mensajes de error de un lenguaje pedagógico **no son log de depuración para el implementador, son interfaz de usuario para el aprendiz**. Un error bueno enseña; un error críptico frustra. Para un niño hispanohablante de 14 años aprendiendo a programar, los mensajes de error son la diferencia entre "este lenguaje me ayuda" y "este lenguaje me odia".

> Un error que no sugiere una corrección concreta es un error mal diseñado.

Esto eleva los mensajes de error a la categoría de **producto**, no de detalle de implementación. Un error mediocre es un bug a corregir, igual que un crash.

### Referentes

- **Rust** (`rustc`): el estándar de la industria desde 2015. Errores que muestran código fuente, indicadores con caret, sugerencias accionables, niveles de detalle expandibles.
- **Elm**: mensajes con tono empático, didácticos, casi conversacionales.
- **Python 3.10+** (PEP 657): mensajes mejorados con caret y sugerencias tipo "did you mean".

Cornamusa adopta el rigor estructural de Rust con la calidez de Elm, en castellano natural.

---

## 2. Anatomía de un mensaje

Todo error producido por Cornamusa tiene esta forma canónica:

```
ErrorDeNombre en programa.cor:5:14
    imprimir(saludaar(nombre))
             ^^^^^^^^
'saludaar' no está definido.
sugerencia: ¿quisiste decir 'saludar'?
```

Componentes obligatorios:
1. **Tipo del error** (`ErrorDeNombre`) — categoría jerárquica de la excepción.
2. **Localización** (`programa.cor:5:14`) — archivo, línea, columna del inicio del fragmento problemático.
3. **Línea de código fuente** — la línea exacta donde está el error.
4. **Indicador con caret** (`^^^^^^^^`) — subraya el rango exacto del problema.
5. **Mensaje principal** — qué pasó, en una frase clara.

Componentes opcionales (y muy recomendados cuando aplican):
6. **Sugerencia** — una corrección concreta, prefijada con `sugerencia:`.
7. **Contexto** — información adicional, prefijada con `nota:` o `ayuda:`.
8. **Cadena de causa** — para errores derivados, prefijada con `causa:`.

### Ejemplo completo (con todos los elementos)

```
ErrorDeTipo en analisis.cor:42:17
    total = items + "iva"
                  ^^
no se puede sumar 'lista' con 'cadena'.
sugerencia: convierte la cadena a número con entero("21") o decimal("0.21").
nota: el operador + concatena cadenas, suma números, y extiende listas, pero
      no mezcla esos tipos. La conversión explícita evita resultados sorpresa.
```

---

## 3. Tono y voz

### Reglas de tono

1. **Tutear siempre.** "tu programa", "lo que escribiste", "intentas hacer". Nunca "su programa" ni "el usuario".
2. **No culpar.** "no encuentro `x`" es mejor que "`x` no existe" (sutil, pero el primero pone la responsabilidad en el compilador, el segundo en el usuario).
3. **Sugerir siempre que se pueda.** Variables similares por edit distance, tipos compatibles, conversiones explícitas, alternativas sintácticas.
4. **Conciso.** Una línea de error principal + una de sugerencia. Detalles adicionales solo cuando aporten.
5. **Sin jerga innecesaria.** "no es iterable" es preferible a "no implementa el protocolo `__iterar__`" (aunque técnicamente sean equivalentes).
6. **Sin emojis** por defecto. Ruido visual en logs, problemas de accesibilidad. Reservar para modo `--amistoso` opcional (a evaluar en v0.5+).

### Comparativa de tono

| ❌ Mal tono | ✅ Buen tono |
|---|---|
| `Error: invalid syntax` | `ErrorDeSintaxis: falta ':' al abrir el bloque` |
| `Se ha producido un error de tipo en línea 5` | `ErrorDeTipo en programa.cor:5:14` |
| `'x' no existe` | `no encuentro 'x'. ¿Quisiste decir 'X'?` |
| `Cannot convert lista to entero` | `no se puede convertir 'lista' a 'entero'` |
| `TypeError: '+' not supported between 'list' and 'str'` | `no se puede sumar 'lista' con 'cadena'` |

---

## 4. Categorías de errores

### 4.1 Errores de sintaxis (`ErrorDeSintaxis`)

Producidos por el lexer y el parser. Aparecen antes de ejecutar el programa.

| Sub-categoría | Cuándo |
|---|---|
| Token inesperado | El parser esperaba algo distinto |
| Cadena sin cerrar | Falta `"` o `'` |
| Paréntesis sin cerrar | Falta `)`, `]` o `}` |
| `fin` desnudo | Anti-pattern de B1: `fin` sin etiqueta |
| Etiqueta de `fin` no coincide | `fin si` en lugar del esperado `fin para` |
| Carácter no reconocido | Símbolo inválido en código fuente |
| Indentación inconsistente | Tabuladores mezclados con espacios |

### 4.2 Errores de nombre y atributo

| Excepción | Cuándo |
|---|---|
| `ErrorDeNombre` | Variable o función no definida en este scope |
| `ErrorDeAtributo` | `obj.x` cuando `x` no existe en `obj` |
| `ErrorDeImportación` | `importar X` cuando `X` no se encuentra |

### 4.3 Errores de tipo y valor

| Excepción | Cuándo |
|---|---|
| `ErrorDeTipo` | Operación inválida entre tipos (ej. `lista + cadena`) |
| `ErrorDeTipo` | Llamar algo que no es callable |
| `ErrorDeTipo` | Argumento de tipo incorrecto en built-in |
| `ErrorDeValor` | Tipo correcto, valor inválido (ej. `entero("hola")`) |

### 4.4 Errores de colección

| Excepción | Cuándo |
|---|---|
| `ErrorDeIndice` | `lista[i]` con `i` fuera de rango |
| `ErrorDeClave` | `dicc[k]` con `k` no presente |

### 4.5 Errores aritméticos

| Excepción | Cuándo |
|---|---|
| `ErrorDivisiónPorCero` | `/`, `//` o `%` con divisor cero |

### 4.6 Errores de ejecución

| Excepción | Cuándo |
|---|---|
| `DesbordeDePila` | Recursión infinita |
| `ErrorRuntime` | Genérico para cualquier otro fallo de ejecución |

### 4.7 Errores de E/S y sistema

| Excepción | Cuándo |
|---|---|
| `ErrorDeIO` | No se puede abrir/leer/escribir archivo |
| `InterrupciónTeclado` | Usuario pulsó Ctrl-C |

---

## 5. Plantillas canónicas

### 5.1 Variable no definida

```
ErrorDeNombre en programa.cor:5:14
    imprimir(saludaar(nombre))
             ^^^^^^^^
no encuentro 'saludaar'.
sugerencia: ¿quisiste decir 'saludar'?
```

**Reglas:**
- Buscar identificadores en scope actual con edit distance ≤ 2.
- Si hay 1 candidato cercano, sugerirlo.
- Si hay varios, listar los 2-3 más cercanos.
- Si no hay candidatos, no sugerir nada (no inventar).

### 5.2 Tipo incorrecto en operación

```
ErrorDeTipo en programa.cor:8:15
    edad = "25" + 1
                ^
no se puede sumar 'cadena' con 'entero'.
sugerencia: convierte la cadena a número con entero("25") o decimal("25.0").
```

**Reglas:**
- Mostrar los dos tipos involucrados con sus nombres castellanos.
- Sugerir conversión explícita si es plausible.
- Si el operador permite múltiples tipos (`+` sirve para `lista + lista`, `cadena + cadena`, números), aclararlo en `nota:`.

### 5.3 Atributo no existe

```
ErrorDeAtributo en programa.cor:12:11
    persona.edaad
            ^^^^^
'Persona' no tiene atributo 'edaad'.
sugerencia: ¿quisiste decir 'edad'?
```

**Reglas:**
- Listar atributos del tipo solo si hay <10 (más es ruido).
- Edit distance ≤ 2 para sugerencias.

### 5.4 Bloque mal cerrado

```
ErrorDeSintaxis en programa.cor:15:1
        retornar resultado
    fin para
    ^^^^^^^^
'fin para' no coincide con el bloque abierto en línea 9.
nota: la línea 9 abrió 'funcion calcular(...)', se esperaba 'fin funcion'.
```

**Reglas:**
- Indicar la línea donde se abrió el bloque.
- Indicar la etiqueta esperada explícitamente.
- Esto requiere mantener una pila de bloques abiertos en el parser.

### 5.5 `fin` desnudo

```
ErrorDeSintaxis en programa.cor:7:1
    si x > 0:
        imprimir(x)
    fin
    ^^^
'fin' requiere una etiqueta.
sugerencia: usa 'fin si' para cerrar el bloque iniciado en línea 5.
```

### 5.6 Falta `:` al abrir bloque

```
ErrorDeSintaxis en programa.cor:3:18
    funcion saludar(nombre)
                            ^
falta ':' después de la cabecera de la función.
sugerencia: las cabeceras de bloque terminan con ':' antes del cuerpo.
```

### 5.7 División por cero

```
ErrorDivisiónPorCero en programa.cor:10:13
    promedio = total / 0
                     ^
no se puede dividir entre cero.
nota: si esperas que el divisor pueda ser cero, prueba con:
    si divisor == 0:
        promedio = 0
    sino:
        promedio = total / divisor
    fin si
```

### 5.8 Índice fuera de rango

```
ErrorDeIndice en programa.cor:6:11
    valor = lista[10]
                  ^^
índice 10 fuera de rango: la lista tiene 5 elementos (índices válidos: 0 a 4).
sugerencia: usa longitud(lista) para conocer el tamaño antes de indexar.
```

### 5.9 Clave no encontrada en diccionario

```
ErrorDeClave en programa.cor:7:18
    edad = personas['Pedro']
                    ^^^^^^^
no encuentro la clave 'Pedro' en el diccionario.
sugerencia: usa 'Pedro' en personas para comprobar antes de acceder, o
            personas.obtener('Pedro', valor_por_defecto) para evitar el error.
```

### 5.10 Argumentos en llamada

```
ErrorDeTipo en programa.cor:11:1
    saludar()
    ^^^^^^^^^
'saludar' espera 2 argumentos pero recibió 0.
nota: definida en programa.cor:3 como 'funcion saludar(nombre, idioma)'.
```

### 5.11 Recursión infinita

```
DesbordeDePila en programa.cor
recursión demasiado profunda (límite: 1000 frames).
nota: tu programa llamó la misma función ~1000 veces sin retorno.
      ¿olvidaste un caso base? Mira la traza de pila:

    factorial → factorial → factorial → ... (997 frames más) → factorial
    fragmentos en programa.cor:5
```

### 5.12 Archivo no encontrado

```
ErrorDeIO al ejecutar 'cornamusa programa.cor'
no encuentro el archivo 'programa.cor' en el directorio actual.
sugerencia: comprueba la ruta y que el archivo existe con
            'ls' (Linux/Mac) o 'dir' (Windows).
```

---

## 6. Anti-patterns explícitos

### ❌ Errores genéricos sin información

```
Error: error grave
```
Razón: información cero. ¿Qué error? ¿Dónde? ¿Por qué?

### ❌ Stack traces gigantes para errores comunes

```
File "interpreter.c", line 1234, in eval_expression
File "interpreter.c", line 1567, in dispatch
File "interpreter.c", line 1890, in run_program
...
```
Razón: es información de implementación, no del programa del usuario. **Excepción:** modo `--depurar` para devs.

### ❌ Mezclar idiomas

```
TypeError: cannot add 'lista' to 'cadena'
```
Razón: si decimos "cadena" debemos decir "ErrorDeTipo: no se puede sumar".

### ❌ Pasivo distante

```
Se ha producido un error en la línea 5
```
Razón: pasivo, vago, formal. No conecta con el usuario.

### ❌ Inventar sugerencias falsas

```
no encuentro 'saludaar'.
sugerencia: ¿quisiste decir 'imprimir'?
```
Razón: si la edit distance es alta, no sugerir. Mejor silencio que mentira.

### ❌ Mensajes que culpan

```
'x' no existe. Has debido equivocarte al escribirlo.
```
Razón: paternalista. El usuario sabe que se ha equivocado, no necesita oírlo.

---

## 7. Internacionalización futura

Cornamusa v0.x-v1.0 produce todos los mensajes **únicamente en castellano**. La internacionalización es:

- **Aplazada explícitamente a v2.0+**.
- **Diseñada desde el principio**: los mensajes viven en una tabla con identificadores (`ERR_NOMBRE_NO_DEFINIDO`, `ERR_TIPO_INCOMPATIBLE`, etc.), no como strings literales en C. Esto facilita traducción futura sin reescribir el motor.
- **Con preferencia por castellano**: si en algún momento se añade i18n, el castellano sigue siendo el idioma "canónico" y el resto son traducciones.

---

## 8. Plan de implementación por fases

| Fase | Mensajes que aplican | Coste implementación |
|---|---|---|
| **v0.2** (lexer) | 5.5, 5.6 (sintaxis básica) + carácter no reconocido + cadena sin cerrar | Estándar definido aquí; ~50 líneas de glue |
| **v0.3** (parser) | 5.4, 5.5, 5.6 + token inesperado (con sugerencia de `fin <etiqueta>`) | Pila de bloques abiertos para mensajes contextuales |
| **v0.4** (tree-walking) | 5.1, 5.2, 5.3, 5.7, 5.8, 5.9, 5.10 (runtime) | Mayoría de errores runtime se introducen aquí |
| **v0.4** | Función auxiliar de **edit distance** para sugerencias "did you mean" | ~30 líneas de C (Levenshtein simple) |
| **v0.5+** | 5.11, 5.12 + refinamiento de mensajes existentes | Iteración basada en feedback de usuarios |

---

## 9. Implementación técnica

### Estructura de un error en C

```c
typedef struct {
    const char *categoria;     // "ErrorDeSintaxis", etc.
    const char *archivo;       // ruta o "<repl>"
    int linea;                 // 1-indexed
    int columna_inicio;        // 1-indexed
    int columna_fin;           // 1-indexed, exclusivo
    const char *texto_linea;   // contenido de la línea de origen
    char *mensaje_principal;   // alocado, requiere free
    char *sugerencia;          // alocado o NULL
    char *nota;                // alocado o NULL
    struct Error *causa;       // cadena de causa, o NULL
} Error;

void emitir_error(const Error *e, FILE *salida);
```

### Función `emitir_error`

Formatea el error según el estándar de la sección 2. Implementación en `errores.{h,c}` (Fase 2 con stub, completo en Fase 4).

### Tabla de mensajes (futuro i18n)

```c
typedef enum {
    ERR_NOMBRE_NO_DEFINIDO,
    ERR_TIPO_INCOMPATIBLE,
    ERR_FIN_DESNUDO,
    /* ... */
} CodigoError;

const char *texto_error(CodigoError codigo);
```

Inicialmente la función devuelve cadenas castellanas hardcoded. En v2.0+ se sustituye por carga desde catálogo de traducciones.

---

## 10. Validación

Cada nuevo error añadido al runtime debe:

1. **Cumplir el formato** de la sección 2.
2. **Tener un test** en `tests/integracion/errores/` con un programa que lo produce y la salida esperada.
3. **Pasar revisión de tono** (tutear, no culpar, sugerir cuando aplica).
4. **Tener entrada en MENSAJES.md** si es un caso nuevo (esta sección 5).

Antes de cada release, ejecutar el script `scripts/auditar_mensajes.sh` (a crear en Fase 2) que verifica:
- Todos los mensajes empiezan con un tipo de error válido.
- Todos incluyen ubicación cuando aplica.
- Ninguno excede 5 líneas (excepto trazas de pila).
- Ninguno contiene texto en inglés.
- Ninguno tiene placeholders sin rellenar (`%s`, `{}`, etc.).
