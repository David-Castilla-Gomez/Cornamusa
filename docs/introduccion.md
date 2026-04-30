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

Cornamusa es **estable** y funcional. Lenguaje completo: OOP con herencia, GC mark-sweep, excepciones, módulos, stdlib mínima. Rendimiento ~3x sobre v0.10 tras small-int tagging (B9). Documentación validada contra el intérprete real.

| Hito | Versión | Estado |
|---|---|---|
| Sintaxis básica | v0.4 | ✅ |
| Estructuras de datos | v0.5 | ✅ |
| VM bytecode + closures + excepciones | v0.6 | ✅ |
| Clases y herencia | v0.7 | ✅ |
| GC mark-sweep | v0.8 | ✅ |
| Módulos + stdlib | v0.9 | ✅ |
| Inline caching tipo PEP 659 | v0.10 | ✅ |
| Small-int tagging | v0.11 | ✅ |
| Documentación + sitio web + ejemplos avanzados | v1.0 | en curso |

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
