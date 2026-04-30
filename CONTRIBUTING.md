# Cómo contribuir a Cornamusa

¡Gracias por tu interés en mejorar Cornamusa! Esta guía describe cómo proponer cambios, reportar fallos y colaborar en el desarrollo.

## Antes de empezar

1. Lee el **[tutorial](docs/tutorial.md)** para entender el lenguaje desde la perspectiva del usuario.
2. Lee la **[especificación formal](ESPEC.md)** si vas a tocar parser/lexer/VM.
3. Revisa las **[decisiones cerradas](decisiones/)** (ADRs `B1`-`B10`): el razonamiento detrás de cada decisión grande del proyecto está documentado allí. Cualquier cambio que vaya a contradecir una ADR existente requiere primero discusión.
4. Revisa los *issues* abiertos para no duplicar trabajo.
5. Para cambios grandes, abre primero un *issue* de discusión.

## Áreas donde se necesita ayuda

- **Diseño del lenguaje:** comentarios sobre `ESPEC.md`, naming castellano natural, propuestas de features.
- **Implementación en C:** lexer (`src/lexer.c`), parser (`src/parser.c`), VM bytecode (`src/vm.c`), compilador (`src/compilador.c`), GC (`src/memoria.c`).
- **Tests:** programas `.cor` que ejerciten casos límite. Especialmente útiles los **tests diferenciales tree-walking vs bytecode** (`tests/CMakeLists.txt` sección `EJEMPLOS_DIFERENCIALES`) que validan que ambos motores producen output idéntico — son la red principal de seguridad ante regresiones semánticas.
- **Documentación:** mejoras al tutorial y referencia en `docs/`. Ejemplos en `examples/`.
- **Biblioteca estándar (`stdlib/*.cor`):** ampliar `matematicas`, `cadenas`, `sistema`; nuevos módulos como `archivos`, `json`, `regex` (planeados v1.1+).
- **Built-ins planeados:** ver §4.2 de `ESPEC.md` para la lista (`leer`, `enumerar`, `mapear`, `filtrar`, etc.).

## Flujo de trabajo

1. Haz un *fork* del repositorio.
2. Crea una rama temática: `git checkout -b mi_caracteristica`.
3. Implementa los cambios siguiendo el estilo del proyecto.
4. Asegúrate de que **todos** los tests pasan: `cmake --build build && cd build && ctest`.
5. Añade tests para tu cambio. Para bugs encontrados, añade un test de regresión (mira `tests/unit/test_bytecode_ic.c::test_regresion_*` como ejemplos).
6. Si el cambio afecta el lenguaje observable (sintaxis, semántica, built-ins), valida también los **tests diferenciales** tree-walking vs bytecode (corren con `ctest -L diferencial`).
7. Actualiza `CHANGELOG.md` bajo la sección `[No publicado]` con una entrada que explique el porqué (no solo el qué — el qué ya está en el diff).
8. Envía un *pull request* con descripción clara que enlace al *issue* original si lo hay.

## Estilo de código

### C

- Estándar: **C11**.
- Indentación: **4 espacios** (sin tabuladores).
- Nombres en `serpiente_minuscula` (snake_case).
- Llaves en línea separada para funciones, en la misma línea para bloques internos.
- Encabezados con guard `#ifndef CORNAMUSA_X_H` / `#define ...` / `#endif`.
- Compilar sin warnings con `-Wall -Wextra -Wpedantic`.

### Cornamusa (`.cor`)

- Indentación: **4 espacios**.
- Identificadores en castellano natural.
- Una sola sentencia por línea.

### Mensajes de commit

Formato breve, en castellano, en imperativo:

```
parser: añadir soporte para listas literales

Implementa la regla de gramática `expr_lista` y los nodos AST
correspondientes. Añade tests para listas vacías, anidadas y con
trailing commas.
```

## Reportar fallos

Abre un *issue* incluyendo:

- Versión de Cornamusa (`cornamusa --version`).
- Sistema operativo y compilador.
- Programa `.cor` mínimo que reproduce el problema.
- Salida observada vs esperada.

## Código de conducta

Este proyecto sigue el [Código de Conducta del Pacto del Colaborador](CODE_OF_CONDUCT.md). Al participar te comprometes a respetarlo.

## Licencia

Al contribuir aceptas que tu trabajo se publique bajo la [licencia MIT](LICENSE).
