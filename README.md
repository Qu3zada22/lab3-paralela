# lab3-paralela

Laboratorio 03 previo al Proyecto 2 — CC3069 Computación Paralela y Distribuida.

**Objetivo:** analizar y mejorar un programa de búsqueda secuencial de claves AES, desarrollar su versión paralela con Open MPI y evaluar su rendimiento mediante tiempos de ejecución y Speedup.

## División del trabajo

### 👤 Persona 1 — Ejercicio 1 (Investigación AES)

- [x] **1a)** Al menos 3 campos de aplicación / ejemplos actuales de uso de AES.
- [x] **1b)** Pasos para cifrar y descifrar con AES-128:
  - Tamaño de la clave y procesamiento en bloques.
  - Principales transformaciones (SubBytes, ShiftRows, MixColumns, AddRoundKey).
- [x] **1c)** Diagrama de flujo de cifrado y descifrado, mostrando cómo interviene la clave.
- [ ] **Extra:** armar el PDF final con las respuestas y capturas de todos.

### 👤 Persona 2 — Ejercicios 2 y 3 (Programa secuencial)

- [ ] **Ej. 2:** Instalar dependencias, compilar, ejecutar y medir el tiempo del programa secuencial. Determinar si usa correctamente AES-128 (con capturas).
  ```bash
  sudo apt update
  sudo apt install libssl-dev
  gcc -std=c11 -O2 -Wall -Wextra busqueda_clave_aes_secuencial.c -o busqueda_clave_aes_secuencial -lcrypto
  ./busqueda_clave_aes_secuencial
  ```
  - [x] Análisis y respuestas en `INFORME_LAB03.md` (sección 2).
  - [ ] Capturas de instalación, compilación y ejecución (tomarlas en Linux/WSL).
- [x] **Ej. 3a:** Identificar errores conceptuales y limitaciones: construcción de la clave, modo ECB, uso del texto original para verificar candidatas, espacio completo de AES-128 vs. rango explorado.
- [x] **Ej. 3b:** Proponer, implementar y justificar mejoras (seguridad, flexibilidad o rendimiento), con el nombre de quien propuso cada una.
  - [ ] Llenar la columna "Propuesta por" en la tabla de mejoras del informe.
  - [ ] Capturas del programa corregido (4 casos de la sección 3 del informe).
- [x] **Entrega:** programa secuencial corregido (`busqueda_clave_aes_secuencial_mejorado.c`), base para la Persona 3.
  ```bash
  gcc -std=c11 -O2 -Wall -Wextra busqueda_clave_aes_secuencial_mejorado.c -o busqueda_clave_aes_secuencial_mejorado -lcrypto
  ./busqueda_clave_aes_secuencial_mejorado [bits=20] [secreta=1000000] [mensaje]
  ```

### 👤 Persona 3 — Ejercicio 4 (Versión paralela con Open MPI)

- [x] **4a)** Versión paralela a partir del secuencial corregido (`busqueda_clave_aes_mpi.c`, informe sección 4):
  - Repartir la búsqueda entre procesos sin omitir ni repetir candidatas.
  - Coordinar la finalización cuando se encuentre la clave o se agote el rango.
  - Verificar que ambas versiones recuperan la misma clave y mensaje.
- [x] **4b)** Medir tiempos con `MPI_Wtime()` usando 2, 3 y 4 procesos y calcular el Speedup.
  - [x] Capturas de compilación, ejecución MPI y mediciones (`imagenes/4.1.png` a `4.6.png`).
  ```bash
  mpicc -std=c11 -O2 -Wall -Wextra busqueda_clave_aes_mpi.c -o busqueda_clave_aes_mpi -lcrypto
  mpirun -np 4 ./busqueda_clave_aes_mpi [bits=20] [secreta=1000000] [mensaje]
  ```

| Cantidad de procesos (n) | Tiempo (s) | Speedup |
|---|---|---|
| 1 (secuencial) | 2.409 | 1.00 |
| 2 | 0.924 | 2.61 |
| 3 | 0.690 | 3.49 |
| 4 | 0.397 | 6.06 |

*Medido con `24 16000000` (2^24 candidatas), ver `imagenes/4.6.png`.*

## Orden de trabajo

1. Persona 1 y Persona 2 avanzan en paralelo; Persona 3 prepara el entorno MPI y la estrategia de distribución.
2. Persona 3 termina la versión MPI cuando Persona 2 entregue el secuencial corregido.
3. Persona 1 integra todo en el PDF final y el grupo lo revisa.

## Entregables

- PDF con respuestas, diagrama y capturas de compilación/ejecución.
- `busqueda_clave_aes_secuencial.c` (original) y `busqueda_clave_aes_secuencial_mejorado.c` (corregido y mejorado).
- Versión paralela con Open MPI.
