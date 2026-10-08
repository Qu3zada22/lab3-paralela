# lab3-paralela

Laboratorio 03 previo al Proyecto 2 — CC3069 Computación Paralela y Distribuida.

**Objetivo:** analizar y mejorar un programa de búsqueda secuencial de claves AES, desarrollar su versión paralela con Open MPI y evaluar su rendimiento mediante tiempos de ejecución y Speedup.

## División del trabajo

### 👤 Persona 1 — Ejercicio 1 (Investigación AES)

- [ ] **1a)** Al menos 3 campos de aplicación / ejemplos actuales de uso de AES.
- [ ] **1b)** Pasos para cifrar y descifrar con AES-128:
  - Tamaño de la clave y procesamiento en bloques.
  - Principales transformaciones (SubBytes, ShiftRows, MixColumns, AddRoundKey).
- [ ] **1c)** Diagrama de flujo de cifrado y descifrado, mostrando cómo interviene la clave.
- [ ] **Extra:** armar el PDF final con las respuestas y capturas de todos.

### 👤 Persona 2 — Ejercicios 2 y 3 (Programa secuencial)

- [ ] **Ej. 2:** Instalar dependencias, compilar, ejecutar y medir el tiempo del programa secuencial. Determinar si usa correctamente AES-128 (con capturas).
  ```bash
  sudo apt update
  sudo apt install libssl-dev
  gcc -std=c11 -O2 -Wall -Wextra busqueda_clave_aes_secuencial.c -o busqueda_clave_aes_secuencial -lcrypto
  ./busqueda_clave_aes_secuencial
  ```
- [ ] **Ej. 3a:** Identificar errores conceptuales y limitaciones: construcción de la clave, modo ECB, uso del texto original para verificar candidatas, espacio completo de AES-128 vs. rango explorado.
- [ ] **Ej. 3b:** Proponer, implementar y justificar mejoras (seguridad, flexibilidad o rendimiento), con el nombre de quien propuso cada una.
- [ ] **Entrega:** programa secuencial corregido (base para la Persona 3).

### 👤 Persona 3 — Ejercicio 4 (Versión paralela con Open MPI)

- [ ] **4a)** Versión paralela a partir del secuencial corregido:
  - Repartir la búsqueda entre procesos sin omitir ni repetir candidatas.
  - Coordinar la finalización cuando se encuentre la clave o se agote el rango.
  - Verificar que ambas versiones recuperan la misma clave y mensaje.
- [ ] **4b)** Medir tiempos con `MPI_Wtime()` usando 2, 3 y 4 procesos y calcular el Speedup (con capturas).

| Cantidad de procesos (n) | Tiempo (s) | Speedup |
|---|---|---|
| 1 (secuencial) | | 1.00 |
| 2 | | |
| 3 | | |
| 4 | | |

## Orden de trabajo

1. Persona 1 y Persona 2 avanzan en paralelo; Persona 3 prepara el entorno MPI y la estrategia de distribución.
2. Persona 3 termina la versión MPI cuando Persona 2 entregue el secuencial corregido.
3. Persona 1 integra todo en el PDF final y el grupo lo revisa.

## Entregables

- PDF con respuestas, diagrama y capturas de compilación/ejecución.
- `busqueda_clave_aes_secuencial.c` (corregido y mejorado).
- Versión paralela con Open MPI.
