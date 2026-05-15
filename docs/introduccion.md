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

Cornamusa es **estable** y maduro. Lenguaje completo con paridad sintáctica cercana a Python 3.10+: OOP con herencia y dunders, closures con `nolocal`, pattern matching, generadores, comprehensions, destructuring, `*args`/`**kwargs`, context managers. GC mark-sweep, excepciones con traceback multi-frame, doce módulos de stdlib. Toda la documentación está validada contra el intérprete real.

| Hito | Versión | Estado |
|---|---|---|
| VM bytecode + closures + excepciones | v0.6 | ✅ |
| Clases, herencia, GC, módulos | v0.7–v0.9 | ✅ |
| Inline caching + small-int tagging | v0.10–v0.11 | ✅ |
| Dunders, `nolocal`, context managers | v1.2–v1.13 | ✅ |
| Pattern matching (`coincidir`) | v1.15–v1.16 | ✅ |
| Stdlib amplia (12 módulos) | v1.8–v1.29 | ✅ |
| Destructuring, `*args`/`**kwargs`, spread | v1.21–v1.25 | ✅ |
| Comprehensions y generadores | v1.30–v1.34 | ✅ |
| Errores con sugerencias + traceback | v1.35–v1.38 | ✅ |
| Performance: `-O3` + LTO | v1.40 | ✅ |
| Dunders de coerción `__repr__` y `__booleano__` | v1.41 | ✅ |

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
