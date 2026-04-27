# Cómo contribuir a Cornamusa

¡Gracias por tu interés en mejorar Cornamusa! Esta guía describe cómo proponer cambios, reportar fallos y colaborar en el desarrollo.

## Antes de empezar

1. Lee la **[especificación del lenguaje](ESPEC.md)** y el **[plan de desarrollo](recursos.md)**.
2. Revisa los *issues* abiertos para no duplicar trabajo.
3. Para cambios grandes, abre primero un *issue* de discusión.

## Áreas donde se necesita ayuda

- **Diseño del lenguaje:** comentarios sobre `ESPEC.md`, sugerencias para naming castellano natural.
- **Implementación en C:** lexer, parser, VM, GC.
- **Tests:** programas `.cor` que ejerciten casos límite.
- **Documentación:** tutoriales, ejemplos, traducción de mensajes de error.
- **Biblioteca estándar:** módulos `matematicas`, `cadenas`, `io`, `json`, etc.

## Flujo de trabajo

1. Haz un *fork* del repositorio.
2. Crea una rama temática: `git checkout -b mi_caracteristica`.
3. Implementa los cambios siguiendo el estilo del proyecto.
4. Asegúrate de que los tests pasan: `make test`.
5. Añade tests para tu cambio.
6. Actualiza `CHANGELOG.md` bajo la sección `[No publicado]`.
7. Envía un *pull request* con descripción clara.

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
