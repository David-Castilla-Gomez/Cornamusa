# B5+B6 — Convención del primer parámetro y nomenclatura de dunders

**Estado:** ✅ **Decidido**.
**Fecha de propuesta:** 2026-04-28
**Fecha de decisión:** 2026-04-28
**Decisor:** David Castilla
**Bloqueadores identificados en:** [REPASO_CRITICO.md](../REPASO_CRITICO.md), problemas B5 y B6.

**Decisión:** Combinación **A1** — `yo` como **convención** (no keyword) para el primer parámetro de métodos de instancia, **dunders en castellano** según la lista canónica de este ADR.

## Por qué un solo ADR

B5 y B6 son técnicamente decisiones independientes, pero **siempre aparecen juntos en código**:

```cornamusa
clase Persona:
    funcion <DUNDER>(<PRIMER_PARAM>, nombre):    # B5 + B6
        <PRIMER_PARAM>.nombre = nombre           # B5
    fin funcion
fin clase
```

Decidirlas por separado obligaría a revisar los ejemplos dos veces. Las analizamos juntas y producimos una sola lista de cambios derivados.

---

## B5 — Convención del primer parámetro

### Contexto

En programación orientada a objetos, los métodos de instancia reciben implícita o explícitamente una referencia a la instancia. Cornamusa la pasa explícitamente como primer parámetro (estilo Python), no implícita (estilo Java/C++/Ruby).

**Cuestión 1 — ¿Es keyword o convención?**
- **Keyword:** el lenguaje fuerza un nombre concreto. Si decides `yo`, escribir `funcion metodo(este, x)` es error de sintaxis.
- **Convención:** el primer parámetro es posicional, el nombre es libre. La comunidad acuerda uno y los formateadores/linters lo sugieren. Python usa este modelo.

**Cuestión 2 — ¿Qué nombre proponemos?**

### Opciones de nombre

| Opción | Longitud | Procedencia | Pros | Contras |
|---|---|---|---|---|
| **`yo`** | 2 | castellano natural | corto, inmediatamente legible, distintivo | suena íntimo/infantil para algunos; pronombre vacío |
| **`self`** | 4 | Python | universalmente familiar a programadores | anglicismo en lenguaje "en castellano" |
| **`este`** | 4 | castellano (este = `this`) | familiar a usuarios de Java/C++/JS | colisiona con demostrativo "este libro"; en código mixto puede confundir |
| **`propio`** | 6 | castellano | técnico, sin ambigüedad | largo, raro como pronombre |
| **`mismo`** | 5 | castellano | técnico, distinto a otras opciones | poco usado en este sentido en castellano |

### Cuestión 1 — keyword vs convención

**Recomendación: convención (estilo Python).**

Razones:
- Permite que los usuarios elijan según preferencia.
- Permite que la stdlib y los ejemplos oficiales muestren coherencia (todos usan el mismo) sin imponerlo.
- Reduce decisiones del lenguaje que pueden resultar incómodas a usuarios concretos.
- Si más adelante queremos pasar a `self` implícito (estilo Java), el cambio es más fácil.

### Cuestión 2 — recomendación de nombre

**Recomendación: `yo` como convención del proyecto** (no keyword).

- 2 letras, mínima fricción al escribir.
- Distintivo: `yo.nombre` se lee "mío punto nombre", inmediatamente claro.
- Refuerza la identidad castellana del lenguaje.
- Aceptable desventaja: algunos lo encontrarán infantil. Pueden escribir `self`, `propio` o lo que prefieran.

**Trade-off explícito:** asumimos cierta incomodidad inicial para usuarios que ya saben Python. A cambio, el código de Cornamusa se ve y se lee distinto desde el primer minuto, no como un "Python con keywords traducidas".

---

## B6 — Nomenclatura de dunders

### Contexto

Los **dunders** (double-underscore methods) son los métodos especiales que el runtime invoca implícitamente: constructor, conversión a string, igualdad, longitud, iteración, operadores aritméticos sobrecargados, etc. Python los nombra en inglés (`__init__`, `__str__`, `__len__`...). Lua, Ruby, JavaScript y C# también.

**Pregunta:** ¿los nombramos en inglés (familiares) o en castellano (coherentes con la filosofía)?

### Opciones

#### Opción A — Inglés (compatibilidad mental con Python)

```cornamusa
clase Persona:
    funcion __init__(yo, nombre):
        yo.nombre = nombre
    fin funcion

    funcion __str__(yo):
        retornar f"Persona({yo.nombre})"
    fin funcion

    funcion __len__(yo):
        retornar longitud(yo.nombre)
    fin funcion
fin clase
```

**Pros:**
- Programadores que migran a/desde Python no reaprenden 30 nombres.
- Documentación puede referenciar literatura Python.
- Tradición universal: ningún lenguaje mainstream localiza dunders.

**Contras:**
- Cornamusa pierde coherencia: el código mezcla castellano y inglés en cada clase.
- Para un niño hispanohablante, `__len__` es jeroglífico, `__longitud__` es legible.
- Filosóficamente, **media decisión**: si nos comprometemos con un lenguaje en castellano, ¿por qué los métodos del runtime no?

#### Opción B — Castellano (coherencia total)

```cornamusa
clase Persona:
    funcion __iniciar__(yo, nombre):
        yo.nombre = nombre
    fin funcion

    funcion __cadena__(yo):
        retornar f"Persona({yo.nombre})"
    fin funcion

    funcion __longitud__(yo):
        retornar longitud(yo.nombre)
    fin funcion
fin clase
```

**Pros:**
- Coherencia total: todo el código se lee en castellano, runtime incluido.
- Pedagógicamente claro: el nombre del método sugiere lo que hace incluso a alguien que no sabe inglés.
- Refuerza la identidad del lenguaje. Si Cornamusa apuesta por castellano, lo apuesta hasta el final.

**Contras:**
- Reaprendizaje al saltar a Python.
- 30+ nombres a estandarizar y traducir cuidadosamente (algunos no son obvios: `__hash__` → `__resumen__`? `__truediv__` → `__dividir__`?).
- Documentación queda como "isla idiomática" sin posibilidad fácil de copy-paste de Python.

#### Opción C — Híbrido (acepta ambos)

El runtime busca primero el nombre castellano, después el inglés. Permite a cada usuario elegir.

**Contras decisivos:**
- Viola "una sola forma evidente".
- Doble nombre por método = doble entrada en tablas hash, doble check en runtime, doble entrada en documentación.
- Las code reviews se llenan de "es `__init__` o `__iniciar__`".

**Recomendación: descartar.**

### Recomendación final B6

**Opción B — castellano**, con una lista canónica completa y bien pensada (sección siguiente).

**Por qué cambiar mi recomendación inicial:**
- En el repaso crítico recomendé inglés "porque son mecanismo del runtime, no API de usuario diaria".
- Repensándolo: el dunder **es código que el usuario escribe** cada vez que define una clase. Aparece en cada clase no trivial. **No es interno del runtime, es parte de la API que ven los programadores.**
- Si decimos `imprimir`, `longitud`, `tipo` (built-ins en castellano), debemos decir `__cadena__`, `__longitud__`, `__tipo__` por coherencia. Cualquier otra cosa es media commit.
- El argumento de "puente a Python" se diluye: alguien que aprende Cornamusa va a Python y tiene que aprender muchísimo más que dunders (built-ins, sintaxis, módulos enteros). 30 nombres extra son trivial frente al cambio total.

### Lista canónica de dunders en castellano

#### Construcción y representación
| Inglés | Castellano | Activación |
|---|---|---|
| `__init__` | `__iniciar__` | constructor (`Persona("Ana")`) |
| `__del__` | `__finalizar__` | destructor (raro de implementar) |
| `__str__` | `__cadena__` | `cadena(obj)`, `f"{obj}"` |
| `__repr__` | `__repr__` *(igual)* | `repr(obj)` — anglicismo aceptado por brevedad |
| `__bool__` | `__booleano__` | `booleano(obj)`, contexto de truthiness |

#### Comparaciones
| Inglés | Castellano | Activación |
|---|---|---|
| `__eq__` | `__igual__` | `a == b` |
| `__ne__` | `__distinto__` | `a != b` |
| `__lt__` | `__menor__` | `a < b` |
| `__le__` | `__menor_igual__` | `a <= b` |
| `__gt__` | `__mayor__` | `a > b` |
| `__ge__` | `__mayor_igual__` | `a >= b` |
| `__hash__` | `__resumen__` | `resumen(obj)` (claves de dict/set) |

#### Colecciones e iteración
| Inglés | Castellano | Activación |
|---|---|---|
| `__len__` | `__longitud__` | `longitud(obj)` |
| `__getitem__` | `__obtener__` | `obj[k]` |
| `__setitem__` | `__establecer__` | `obj[k] = v` |
| `__delitem__` | `__borrar__` | `borrar obj[k]` |
| `__contains__` | `__contiene__` | `x en obj` |
| `__iter__` | `__iterar__` | iteración (`para x en obj:`) |
| `__next__` | `__siguiente__` | `siguiente(it)` |

#### Aritméticos
| Inglés | Castellano | Activación |
|---|---|---|
| `__add__` | `__sumar__` | `a + b` |
| `__sub__` | `__restar__` | `a - b` |
| `__mul__` | `__multiplicar__` | `a * b` |
| `__truediv__` | `__dividir__` | `a / b` |
| `__floordiv__` | `__div_entera__` | `a // b` |
| `__mod__` | `__modulo__` | `a % b` |
| `__pow__` | `__potencia__` | `a ** b` |
| `__neg__` | `__negar__` | `-a` |
| `__pos__` | `__positivar__` | `+a` |
| `__abs__` | `__absoluto__` | `absoluto(a)` |

#### Llamada y contexto
| Inglés | Castellano | Activación |
|---|---|---|
| `__call__` | `__llamar__` | `obj(args)` |
| `__enter__` | `__entrar__` | `con obj como ...` (futuro) |
| `__exit__` | `__salir__` | `con obj como ...` (futuro) |

#### Atributos dinámicos
| Inglés | Castellano | Activación |
|---|---|---|
| `__getattr__` | `__obtener_atributo__` | `obj.x` cuando no existe |
| `__setattr__` | `__establecer_atributo__` | `obj.x = v` |
| `__delattr__` | `__borrar_atributo__` | `borrar obj.x` |

**Total: ~32 dunders canónicos.** Algunos preservan forma inglesa por brevedad y universalidad (`__repr__`, `__hash__` candidato pero proponemos `__resumen__`). El resto en castellano.

**Excepción razonada — `__repr__`**: `representar` es largo, ya tenemos `__cadena__` para el caso usual, y `repr` es usado universalmente en literatura. Justificable como préstamo. Alternativa más larga: `__representar__`.

---

## Decisiones combinadas — las 4 visualizaciones

Mismo programa con cada combinación:

### Combinación A1 — `yo` + dunders en castellano *(recomendada)*

```cornamusa
clase Pila:
    funcion __iniciar__(yo):
        yo.elementos = []
    fin funcion

    funcion __longitud__(yo):
        retornar longitud(yo.elementos)
    fin funcion

    funcion __cadena__(yo):
        retornar f"Pila({yo.elementos})"
    fin funcion

    funcion apilar(yo, x):
        yo.elementos.añadir(x)
    fin funcion

    funcion desapilar(yo):
        retornar yo.elementos.quitar()
    fin funcion
fin clase
```

### Combinación A2 — `yo` + dunders en inglés

```cornamusa
clase Pila:
    funcion __init__(yo):
        yo.elementos = []
    fin funcion

    funcion __len__(yo):
        retornar longitud(yo.elementos)
    fin funcion

    funcion __str__(yo):
        retornar f"Pila({yo.elementos})"
    fin funcion
fin clase
```

### Combinación B1 — `self` + dunders en castellano

```cornamusa
clase Pila:
    funcion __iniciar__(self):
        self.elementos = []
    fin funcion

    funcion __longitud__(self):
        retornar longitud(self.elementos)
    fin funcion
fin clase
```

### Combinación B2 — `self` + dunders en inglés *(idéntico a Python)*

```cornamusa
clase Pila:
    funcion __init__(self):
        self.elementos = []
    fin funcion

    funcion __len__(self):
        retornar longitud(self.elementos)
    fin funcion
fin clase
```

---

## Recomendación final combinada

**Combinación A1: `yo` (convención, no keyword) + dunders en castellano.**

**Justificación:**
- Coherencia total: `imprimir`, `longitud`, `__longitud__`, `yo.nombre` — todo castellano.
- Identidad propia: Cornamusa **no es Python en castellano**, es un lenguaje en castellano que se parece a Python.
- Pedagógicamente óptimo para hispanohablantes principiantes.
- El "coste" de no ser idéntico a Python lo pagamos a cambio de ser un lenguaje completo, no medio compromiso.

**Anti-recomendación: B2** (`self` + inglés). Es Python con keywords traducidas, sin valor añadido. Si alguien quiere eso, mejor que use Python directamente.

---

## Decisión

**Combinación A1 adoptada el 2026-04-28.**

- **B5:** El primer parámetro de los métodos de instancia es **`yo` por convención**, no por keyword. El nombre del parámetro es libre — la stdlib y los ejemplos oficiales usan `yo` consistentemente.
- **B6:** Los dunders se nombran en **castellano** según la lista canónica de este ADR. El runtime invoca solo los nombres castellanos; los nombres ingleses de Python (`__init__`, `__len__`, etc.) no son reconocidos.

### Excepción razonada: `__repr__` se mantiene

Tras revisión, mantenemos `__repr__` como dunder con nombre inglés. Razones:
- `representar` es notablemente más largo que el resto.
- `repr` es préstamo aceptado en literatura técnica castellana.
- La función built-in también se llama `repr(x)`.
- Coherente con que `__repr__` se invoque rara vez por el usuario (suele ser interno del REPL/debugger).

Esta excepción está documentada en la tabla de §4 con la nota "préstamo aceptado por brevedad". Si en v0.5 detectamos que es incoherente, se renombra a `__representar__` con cambio menor.

---

## Consecuencias (cuando se decida)

**Si se decide A1 (`yo` + castellano)** — recomendación:
- ESPEC §2.3: aclarar que `yo` es **convención**, no keyword reservada (eliminar de la tabla, mover a sección "convenciones").
- ESPEC §4: tabla de dunders actualizada con la lista canónica castellana.
- ESPEC §6.6: documentar que dunders castellanos son los únicos reconocidos por el runtime.
- `examples/`: revisar los 12 ejemplos. Los que ya tienen `yo` y dunders castellanos (`07_clases_herencia`, `11_iterador`) están alineados; los demás no necesitan cambio.
- `decisiones/B5-B6-...md`: registrar decisión.
- CHANGELOG: entradas para B5 y B6.

**Si se decide A2 (`yo` + inglés):**
- Reescribir 2 ejemplos (`07_clases_herencia` cambia `__iniciar__`→`__init__`, `__cadena__`→`__str__`; `11_iterador` cambia `__iterar__`→`__iter__`, `__siguiente__`→`__next__`).
- ESPEC §4 y §6.6: nombres de dunders en inglés.

**Si se decide B1 o B2 (`self`):**
- Reescribir todos los ejemplos que usan `yo` (la mayoría de los con clases o métodos).
