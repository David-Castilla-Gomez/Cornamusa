# Referencia verificada de la biblioteca estándar de Cornamusa

> Esta referencia se generó en la fase 5 del plan de corrección del corpus
> (v1.122). Cada nombre listado corresponde a una función o clase que
> **existe** en `stdlib/*.cor` con la firma exacta mostrada. Los nombres
> verificados con un `.cor` que los invoca y se ejecuta sin error
> (`tests/golden_referencia_stdlib`).
>
> Notas comunes:
>
> - Toda función con `*` o `**` en los args usa la convención `*args` /
>   `**kw` de Cornamusa (paridad con Python).
> - Parámetros con `= valor` tienen ese default.
> - Nombres prefijados con `_` son privados del módulo (no documentados).
> - Para los **métodos sobre tipos nativos** (lista, cadena, dict, conjunto)
>   ver el final del documento — esos NO requieren importar nada.

---

## `archivos`

```cornamusa
importar archivos
```

| Función | Descripción |
|---|---|
| `leer(ruta)` | Lee el archivo entero como cadena. Lanza `ErrorDeIO`. |
| `escribir(ruta, contenido)` | Sobrescribe. |
| `lineas(ruta)` | Lista con cada línea (sin `\n`). |
| `existe(ruta)` | booleano. |
| `agregar(ruta, contenido)` | Append. |
| `es_directorio(ruta)` | booleano. |
| `listar(ruta)` | Nombres de entradas (sin recursión). |
| `directorio_actual()` | cwd. |
| `crear_directorio(ruta)` | mkdir. |
| `eliminar(ruta)` | Borra archivo. |
| `eliminar_directorio(ruta)` | rmdir (vacío). |
| `info(ruta)` | dict con `tamano`, `mtime_ms`, `es_dir`. |
| `eliminar_arbol(ruta)` | rm -rf. |
| `copiar(origen, destino)` | Copia archivo. |
| `mover(origen, destino)` | Rename atómico. |
| `set_mtime(ruta, mtime_ms)` | Cambia mtime. |
| `tocar(ruta)` | Crea si no existe; actualiza mtime. |
| `copiar_arbol(origen, destino)` | cp -r. |
| `crear_arbol(ruta)` | mkdir -p. |

## `argumentos`

Argparse-style. Clase `Parser` con `agregar_arg`, `parsear(args)`.

## `azar`

```cornamusa
importar azar
```

| Función | Descripción |
|---|---|
| `decimal()` | float en `[0, 1)`. |
| `entero(a, b)` | entero en `[a, b]`. |
| `semilla(n)` | Fija seed. |
| `elegir(seq)` | Elemento aleatorio. |
| `barajar(lista)` | Mutates in-place. |
| `muestra(seq, k)` | k elementos sin reposición. |
| `booleano(p=0.5)` | Bernoulli. |
| `uniforme(a, b)` | float en `[a, b]`. |
| `normal(mu, sigma)` | Box-Muller. |
| `exponencial(tasa)`, `binomial(n, p)`, `poisson(media)` | Distribuciones (v1.110). |

## `base64`

`codificar(s)`, `decodificar(s)`, `codificar_url(s)`, `decodificar_url(s)`.

## `cadenas`

> **Nombres clave** (causa común de errores en el corpus):
>
> - `mayusculas_ascii(s)`, `minusculas_ascii(s)` — sí, con sufijo `_ascii`.
> - `separar(s, sep)` — NO `dividir`.
> - `unir(partes, sep)`.

| Función | Descripción |
|---|---|
| `repetir(s, n)` | Concatena `s` n veces. |
| `es_vacia(s)` | booleano. |
| `unir(partes, sep)` | Como `sep.join(partes)`. |
| `empieza_con(s, prefijo)` / `termina_con(s, sufijo)` | booleano. |
| `indice_de(s, sub)` | `-1` si no encuentra. |
| `contiene(s, sub)` | booleano. |
| `separar(s, sep)` | Split. |
| `reemplazar(s, viejo, nuevo)` | Replace all. |
| `minusculas_ascii(s)` / `mayusculas_ascii(s)` | Solo ASCII. |
| `recortar(s)`, `recortar_izquierda(s)`, `recortar_derecha(s)` | trim. |
| `contar(s, sub)` | Ocurrencias no solapadas. |

## `coleccion`

Clases con API tipo Python:

- `Pila()` — LIFO. `poner(x)`, `sacar()`, `vista()`, `vacia()`, `longitud(p)`.
- `Cola()` — FIFO. Mismos métodos.
- `ColaDoble()` — `poner_frente/final`, `sacar_frente/final`, `vista_frente/final`.
- `Heap(clave=nulo)` (v1.116, clave en v1.120) — min-heap. `poner(x)`, `sacar()`, `vista()`. `clave=lambda` permite tuplas/dicts.
- `Contador(items?=nulo)` — multiset. `incrementar(x, n=1)`, `decrementar`, `obtener(x)`, `mas_comunes(n=nulo)`, `total()`, `items()`.

## `csv`

`parsear(texto, sep=",")`, `parsear_linea(linea, sep=",")`, `serializar_linea(campos, sep=",")`, `serializar(filas, sep=",")`, `leer(ruta, sep=",")`, `escribir(ruta, filas, sep=",")`.

## `estadisticas` (v1.117)

`media(xs)`, `mediana(xs)`, `mediana_baja/alta(xs)`, `moda(xs)`, `multimodal(xs)`, `media_armonica(xs)`, `media_geometrica(xs)`, `varianza(xs)` (muestral), `varianza_pob(xs)`, `desviacion(xs)`, `desviacion_pob(xs)`, `amplitud(xs)` (NO `rango`), `percentil(xs, p)`, `cuartiles(xs)`, `covarianza(xs, ys)`, `correlacion(xs, ys)` (Pearson), `regresion_lineal(xs, ys)` → dict, `resumen(xs)` → dict.

## `fechas`

`ahora()`, `componentes(ts)`, `construir(año, mes, dia, hora=0, minuto=0, segundo=0)`, `formato(ts, spec)`, `iso8601(ts)`, `legible(ts)`, `solo_fecha(ts)`, `solo_hora(ts)`, `sumar_dias(ts, n)`, `sumar_horas(ts, n)`, `diferencia_seg(a, b)`, `diferencia_dias(a, b)`, `es_bisiesto(año)`, `dias_en_mes(año, mes)`, `nombre_dia(d)`, `nombre_mes(m)`.

## `formato`

`rellenar(s, ancho, caracter=" ")`, `alinear_derecha(s, ancho, caracter=" ")`, `centrar(s, ancho, caracter=" ")`, `con_decimales(n, decimales=2)`, `numero_con_separador(n, separador="_")`, `porcentaje(d, decimales=2)`, `como_hex(n, prefijo="0x")`, `como_binario(n, prefijo="0b")`, `linea(caracter="-", ancho=60)`, `fila(valores, anchos, separador=" | ")`.

## `funcionales`

`mapear(f, xs)`, `filtrar(p, xs)`, `reducir(f, xs, inicial)`, `enumerar(xs, inicio=0)`, `cualquiera(p, xs)`, `todos(p, xs)`, `suma(xs, inicial=0)`, `minimo(xs)`, `maximo(xs)`, `agrupar_por(xs, f)`, `tomar(n, xs)`, `saltar(n, xs)`, `combinar(xs, ys)`, `aplanar(xs)`, `unicos(xs)`, `ordenar_por(xs, clave)`, `ordenar_por_inverso(xs, clave)`.

## `grafos` (v1.119)

Clase `Grafo(dirigido=verdadero)`:

- `agregar_nodo(n)`, `agregar_arista(u, v, peso=1)`, `quitar_arista(u, v)`.
- `nodos()`, `aristas()`, `vecinos(n)`, `peso(u, v)`, `contiene(n)`.

Algoritmos:

- `bfs(g, inicio)` → lista de visita.
- `dfs(g, inicio)` → lista preorden.
- `dijkstra(g, inicio)` → dict `nodo → distancia`.
- `camino_mas_corto(g, inicio, finn)` → lista de nodos.
- `componentes(g)` → lista de listas.
- `topologico(g)` → Kahn. Lanza si hay ciclo.
- `tiene_ciclo(g)` → booleano.

## `hashing`

`sha256(s)`, `md5(s)`, `hmac_sha256(clave, mensaje)`, `hmac_sha256_bytes(clave, mensaje)`, `hmac_md5(clave, mensaje)`.

## `inspeccion`

`obtener_clase(x)`, `obtener_nombre(x)`, `listar_metodos(x)`, `listar_atributos(x)`, `es_callable(x)`, `es_clase(x)`, `es_instancia(x)`, `es_modulo(x)`, `describir(x)`.

## `iteradores` (v1.118)

Combinatoria: `producto(xs, ys)`, `producto3(xs, ys, zs)`, `producto_repeticion(xs, r)`, `permutaciones(xs, r=-1)`, `combinaciones(xs, r)`, `combinaciones_con_repeticion(xs, r)`.

Iteración: `concatenar(xs, ys)` (NO `cadena`, sombrearía built-in), `repetir(valor, n)`, `ventana(xs, n)`, `pares_consecutivos(xs)`, `agrupar_consecutivos(xs)`, `comprimir(xs, selectores)`, `dividir_en(xs, n)`.

## `json`

`parsear(texto)`, `serializar(valor)`, `serializar_indentado(valor, indent)`.

## `jwt`

`codificar(payload, clave)`, `decodificar(token, clave)`, `verificar(token, clave)`, `expirado(payload, ahora)`, `decodificar_y_validar(token, clave, ahora)`.

## `matematicas`

Constantes: `PI`, `E`, `TAU`, `INFINITO`, `NO_NUMERO`.

Funciones aritméticas: `cuadrado(n)`, `cubo(n)`, `absoluto(n)`, `maximo(a, b)`, `minimo(a, b)`, `signo(n)`, `factorial(n)`, `suma_rango(a, b)`, `es_par(n)`, `es_impar(n)`, `mcd(a, b)`, `raiz(x)`, `ln(x)`, `log10(x)`, `log(x, base)`, `exp(x)`, `potencia(x, expo)`.

Trigonometría: `seno(x)` (NO `sen`), `coseno(x)`, `tangente(x)`, `arco_seno(x)`, `arco_coseno(x)`, `arco_tangente(x)`, `arco_tangente2(dy, dx)`, `grados_a_radianes(g)`, `radianes_a_grados(r)`, `hipotenusa(a, b)`.

Redondeo: `techo(x)`, `suelo(x)`, `redondear(x)`.

Predicados de IEEE 754: `es_infinito(x)`, `es_no_numero(x)`, `es_finito(x)`.

## `proceso`

`ejecutar(programa, *args)`, `capturar(programa, *args)`, `codigo(programa, *args)`.

## `pruebas`

Framework de testing. `aseverar*` para asserts; clase `Suite` para agrupar.

## `red`

`obtener(url, cabeceras_extra=nulo, timeout=10)`, `descargar_cuerpo(url)`, `parsear_cabeceras(cab_raw)`.

## `regex`

`coincide(patron, texto)`, `buscar(patron, texto)`, `todos(patron, texto)`, `reemplazar(patron, texto, rep)`, `contiene(patron, texto)`, `extraer(patron, texto)`.

## `ruta`

`es_absoluta(s)`, `nombre(s)`, `tronco(s)`, `extension(s)`, `padre(s)`, `partes(s)`, `unir_partes(lista)`, `normalizar(s)`, `recorrer(dir)`, `encontrar(dir, patron)`. Clase `Ruta(s)`.

## `sistema`

`obtener_variable(nombre)`, `establecer_variable(nombre, valor)`, `variables()`, `inicio()` (home), `usuario()`, `host()`, `directorio_temp()`. (NO `argv` directamente — usa el built-in global `obtener_argv()`.)

## `tiempo`

`epoch_segundos()`, `epoch_ms()`, `monotonic()`, `dormir(s)`. Clase `Cronometro`, helper `cronometro()`.

## `validacion`

`es_email(s)`, `es_url(s)`, `es_fecha_iso(s)`, `es_telefono(s)`, `en_rango(n, lo, hi)`, `en_rango_abierto(n, lo, hi)`, `longitud_en_rango(s, min, max)`, `no_vacia(s)`, `coincide(s, patron)`, `en_conjunto(x, valores)`. Clase `Validador`.

---

## Métodos sobre tipos nativos (sin importar)

Los tipos built-in exponen métodos directamente. Despachados a las nativas globales subyacentes en la tabla `METODOS_NATIVOS` de `src/nativos.c`.

### `lista`

- `agregar(x)` / `añadir(x)` (alias) — append.
- `insertar(i, x)`, `quitar(i=-1)`, `ordenar()`, `invertir()`.
- (v1.122) `contar(x)` — cuenta apariciones por igualdad.
- (v1.122) `contiene(x)` — booleano (equivalente al operador `x en xs`).
- (v1.122) `copiar()` — shallow copy.
- (v1.128) `indice_de(x)` — devuelve el índice de la primera aparición o `-1`.

### `cadena`

- `minusculas()` / `mayusculas()` — solo ASCII.
- `empieza_con(s)`, `termina_con(s)`, `indice_de(s)`.
- (v1.122) `separar(sep)` — split. `sep=""` separa por code-point.
- (v1.122) `reemplazar(viejo, nuevo)` — replace all. O(n). `viejo=""` devuelve la cadena tal cual.
- (v1.122) `recortar()` — trim de espacios ASCII en ambos extremos (` `, `\t`, `\n`, `\r`, `\f`, `\v`).
- (v1.122) `contiene(sub)` — booleano.
- (v1.122) `unir(lista)` — receptor es el separador: `",".unir(["a","b"]) == "a,b"`.

### `diccionario`

- `claves()`, `valores()`.
- (v1.122) `items()` — lista de `[clave, valor]` en orden de inserción. Iterable con `para par en d.items():`.
- (v1.122) `obtener(clave, defecto)` — devuelve el valor o `defecto` si no existe. NO lanza `ErrorDeClave`.

### `conjunto` (v1.128)

- `agregar(x)` / `añadir(x)` (alias) — añade elemento (sin duplicar).
- `quitar(x)` — elimina; lanza `ErrorDeValor` si no estaba.
- `union(otro)` — conjunto nuevo con todos los elementos de ambos.
- `interseccion(otro)` — conjunto nuevo con los elementos comunes. O(min).
- `diferencia(otro)` — conjunto nuevo con los elementos en `self` pero no en `otro`.
- `es_subconjunto(otro)` — booleano, `self ⊆ otro`.
- `contiene(x)` — booleano. Si `x` no es hashable devuelve `falso` sin lanzar.
- `copiar()` — shallow copy.

### `tupla` (v1.128)

- `contar(x)` — apariciones por igualdad.
- `contiene(x)` — booleano (equivalente al operador `x en t`).
- `indice_de(x)` — índice de la primera aparición o `-1`.

---

## Excepciones built-in

Lanzables y atrapables sin importar nada:

`Excepcion`, `ErrorAritmetico`, `ErrorDeTipo`, `ErrorDeValor`, `ErrorDeIndice`, `ErrorDeClave`, `ErrorDeNombre`, `ErrorDeSistema`, `ErrorDeIO`, `ErrorDeIteracion`, `ErrorDeAtributo`.

> No existe `ErrorDivisiónPorCero` (división por cero lanza `ErrorAritmetico`). No existe `ErrorRuntime` (usar `Excepcion` para abstractos).
