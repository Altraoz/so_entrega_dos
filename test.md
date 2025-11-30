# Entradas para testeo

## Caso 1
| Entrada | Memoria |Estado | Acción |
| ------------ | ------------ | ------------ | ------------ |
| 1000 | [x,_,_,_,_] | Miss | se llena slot[0] |
| 2000 | [x,x,_,_,_] | Miss | se llena slot[1] |
| 3000 | [x,x,x,_,_] | Miss | se llena slot[2] |
| 4000 | [x,x,x,x,_] | Miss | se llena slot[3] |
| 5000 | [x,x,x,x,x] | Miss | se llena slot[4], Aquí memo full |
| 6000 | [x,x,x,x,x] | Miss | se reemplaza en el mas antiguo |


## Caso 2
| Entrada | Memoria       | Estado | Acción                             |
|---------|---------------|--------|------------------------------------|
| 1000    | [x, _, _, _, _] | Miss   | se llena slot[0]                  |
| 2000    | [x, x, _, _, _] | Miss   | se llena slot[1]                  |
| 3000    | [x, x, x, _, _] | Miss   | se llena slot[2]                  |
| 4000    | [x, x, x, x, _] | Miss   | se llena slot[3]                  |
| 5000    | [x, x, x, x, x] | Miss   | se llena slot[4], Aquí memo full  |
| 6000    | [6000, x, x, x, x] | Miss   | se reemplaza en el más antiguo (slot[0]) |

## Caso 3
| Entrada | Memoria       | Estado | Acción                             |
|---------|---------------|--------|------------------------------------|
| 1000    | [x, _, _, _, _] | Miss   | se llena slot[0]                  |
| 2000    | [x, x, _, _, _] | Miss   | se llena slot[1]                  |
| 3000    | [x, x, x, _, _] | Miss   | se llena slot[2]                  |
| 4000    | [x, x, x, x, _] | Miss   | se llena slot[3]                  |
| 5000    | [x, x, x, x, x] | Miss   | se llena slot[4], Aquí memo full  |
| 6000    | [6000, 2000, x, x, x] | Miss   | se reemplaza en el más antiguo (slot[0]) |

## Caso 4
| Entrada | Memoria       | Estado | Acción                             |
|---------|---------------|--------|------------------------------------|
| 1000    | [x, _, _, _, _] | Miss   | se llena slot[0]                  |
| 2000    | [x, x, _, _, _] | Miss   | se llena slot[1]                  |
| 3000    | [x, x, x, _, _] | Miss   | se llena slot[2]                  |
| 4000    | [x, x, x, x, _] | Miss   | se llena slot[3]                  |
| 5000    | [x, x, x, x, x] | Miss   | se llena slot[4], Aquí memo full  |
| 6000    | [6000, 2000, 1000, x, x] | Miss   | se reemplaza en el más antiguo (slot[0]) |

## Caso 5
| Entrada | Memoria       | Estado | Acción                             |
|---------|---------------|--------|------------------------------------|
| 1000    | [x, _, _, _, _] | Miss   | se llena slot[0]                  |
| 2000    | [x, x, _, _, _] | Miss   | se llena slot[1]                  |
| 3000    | [x, x, x, _, _] | Miss   | se llena slot[2]                  |
| 4000    | [x, x, x, x, _] | Miss   | se llena slot[3]                  |
| 5000    | [x, x, x, x, x] | Miss   | se llena slot[4], Aquí memo full  |
| 6000    | [6000, 2000, 3000, x, x] | Miss   | se reemplaza en el más antiguo (slot[0]) |







1000
2000
3000
4000
5000
6000
500
10000
11000
12000


en 10000 falla y arroja la misma posición de reemplazo que 500


en realidad sucede no solamente con cuando ponemos antes un numero 500
el problema al probar 500,501,502,5003. todos se reemplazaban en el mismo sitio.
por que ? 

el error de repetidas se da si el nuevo valor a ingresar es menro al menor presente.
en este punto se imprime en el mismo sitio  no en el menos usado



# OLIVDEN TODO QUEDÓ SOLUCIONADO