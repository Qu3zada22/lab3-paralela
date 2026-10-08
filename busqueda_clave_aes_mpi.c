/*----------------------------------------------------------------------
 * UNIVERSIDAD DEL VALLE DE GUATEMALA
 * Curso:       CC3069 - Computacion Paralela y Distribuida
 * Ejercicio:   Busqueda paralela de clave AES-128 con Open MPI
 *
 * Parte de busqueda_clave_aes_secuencial_mejorado.c y conserva sus
 * mejoras (CBC con IV aleatorio, argumentos, contexto EVP reutilizado,
 * verificacion con el primer bloque y reporte completo).
 *
 * Distribucion CICLICA: el proceso r prueba las candidatas
 *   r, r + size, r + 2*size, ...
 * Cada candidata 0..2^bits-1 pertenece a exactamente un proceso (sin
 * omitir ni repetir) y la carga queda balanceada sin importar donde
 * este la clave.
 *
 * Finalizacion: cada CHECK_INTERVAL iteraciones todos los procesos hacen
 * MPI_Allreduce(MIN) de su hallazgo local; si alguno encontro la clave,
 * todos se detienen. Si nadie la encuentra, el ciclo termina al agotar
 * el rango. El MIN devuelve la misma clave que la version secuencial
 * (la menor que coincide).
 *
 * Uso: mpirun -np N ./busqueda_clave_aes_mpi [bits=20] [secreta=1000000]
 *                                           [mensaje="Puedes lograrlo!"]
 *----------------------------------------------------------------------*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <mpi.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#define BLOCK_LEN 16
#define KEY_LEN 16
#define MAX_MESSAGE_LEN 1024
#define DEFAULT_BITS 20
#define MAX_BITS 40
#define DEFAULT_SECRET UINT64_C(1000000)
#define DEFAULT_MESSAGE "Puedes lograrlo!"
#define CHECK_INTERVAL UINT64_C(4096)

/* Muestra el error y aborta todos los procesos. */
static void fail(const char *description)
{
    fprintf(stderr, "%s\n", description);
    ERR_print_errors_fp(stderr);
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    exit(EXIT_FAILURE);
}

/* Misma construccion de clave que la version secuencial. */
static void make_key(uint64_t candidate, unsigned char key[KEY_LEN])
{
    memset(key, 0, KEY_LEN);
    for (int i = 0; i < 8; i++) {
        key[KEY_LEN - 1 - i] = (unsigned char)((candidate >> (8 * i)) & 0xFF);
    }
}

/* Configura el contexto una sola vez (AES-128-CBC y relleno). */
static EVP_CIPHER_CTX *ctx_create(int encrypt, int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (ctx == NULL) {
        fail("No se pudo crear el contexto de OpenSSL.");
    }
    if (EVP_CipherInit_ex(ctx, EVP_aes_128_cbc(), NULL,
                          NULL, NULL, encrypt) != 1) {
        fail("Error al inicializar AES-128-CBC.");
    }
    if (EVP_CIPHER_CTX_set_padding(ctx, padding) != 1) {
        fail("Error al configurar el relleno.");
    }
    return ctx;
}

/*
 * Procesa 'len' bytes con la candidata dada; el contexto ya esta listo.
 * Devuelve la cantidad de bytes escritos, o -1 si OpenSSL rechaza el
 * resultado (por ejemplo, relleno invalido al descifrar).
 */
static int crypt_buffer(
    EVP_CIPHER_CTX *ctx, uint64_t candidate,
    const unsigned char iv[BLOCK_LEN],
    const unsigned char *input, int len, unsigned char *output)
{
    unsigned char key[KEY_LEN];
    int written = 0;
    int final_written = 0;

    make_key(candidate, key);

    if (EVP_CipherInit_ex(ctx, NULL, NULL, key, iv, -1) != 1) {
        fail("Error al cargar la clave.");
    }
    if (EVP_CipherUpdate(ctx, output, &written, input, len) != 1) {
        fail("Error al procesar los datos.");
    }
    if (EVP_CipherFinal_ex(ctx, output + written, &final_written) != 1) {
        return -1;
    }
    return written + final_written;
}

/* Convierte un argumento a entero sin signo, validando errores. */
static uint64_t parse_u64(const char *s, const char *name, int rank)
{
    char *end = NULL;

    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || s[0] == '-') {
        if (rank == 0) {
            fprintf(stderr, "Argumento invalido para %s: '%s'\n", name, s);
        }
        MPI_Finalize();
        exit(EXIT_FAILURE);
    }
    return (uint64_t)v;
}

static void print_hex(const unsigned char *data, int len)
{
    for (int i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    putchar('\n');
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    uint64_t bits = DEFAULT_BITS;
    uint64_t secret = DEFAULT_SECRET;
    const char *message = DEFAULT_MESSAGE;

    /* Todos los procesos validan igual, asi salen juntos si hay error. */
    if (argc > 4) {
        if (rank == 0) {
            fprintf(stderr, "Uso: %s [bits] [secreta] [mensaje]\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }
    if (argc > 1) bits = parse_u64(argv[1], "bits", rank);
    if (argc > 2) secret = parse_u64(argv[2], "secreta", rank);
    if (argc > 3) message = argv[3];

    if (bits < 1 || bits > MAX_BITS) {
        if (rank == 0) {
            fprintf(stderr, "bits debe estar entre 1 y %d.\n", MAX_BITS);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    const int message_len = (int)strlen(message);
    if (message_len < 1 || message_len > MAX_MESSAGE_LEN) {
        if (rank == 0) {
            fprintf(stderr, "El mensaje debe tener entre 1 y %d bytes.\n",
                    MAX_MESSAGE_LEN);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    const uint64_t total_keys = UINT64_C(1) << bits;
    if (rank == 0 && secret >= total_keys) {
        printf("Aviso: la clave secreta esta fuera del rango 2^%" PRIu64
               "; se recorrera todo el rango sin encontrarla.\n", bits);
    }

    /*
     * El proceso 0 genera el IV y cifra el mensaje; luego difunde el IV
     * y el texto cifrado. Los demas procesos nunca usan la clave secreta.
     */
    unsigned char iv[BLOCK_LEN];
    unsigned char cipher[MAX_MESSAGE_LEN + BLOCK_LEN];
    int cipher_len = 0;

    if (rank == 0) {
        if (RAND_bytes(iv, sizeof(iv)) != 1) {
            fail("No se pudo generar el IV.");
        }
        EVP_CIPHER_CTX *ctx_enc = ctx_create(1, 1);
        cipher_len = crypt_buffer(
            ctx_enc, secret, iv,
            (const unsigned char *)message, message_len, cipher);
        EVP_CIPHER_CTX_free(ctx_enc);
        if (cipher_len < 0) {
            fail("Error al cifrar el mensaje.");
        }
    }
    MPI_Bcast(iv, BLOCK_LEN, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(&cipher_len, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(cipher, cipher_len, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    /* Fragmento conocido: primer bloque del texto plano con relleno. */
    unsigned char known_block[BLOCK_LEN];
    if (message_len >= BLOCK_LEN) {
        memcpy(known_block, message, BLOCK_LEN);
    } else {
        const unsigned char pad = (unsigned char)(BLOCK_LEN - message_len);
        memcpy(known_block, message, (size_t)message_len);
        memset(known_block + message_len, pad, BLOCK_LEN - (size_t)message_len);
    }

    /*
     * Todos los procesos hacen la misma cantidad de iteraciones,
     * ceil(total_keys / size), para que los MPI_Allreduce coincidan.
     * Las candidatas que se pasan del rango simplemente se saltan.
     */
    const uint64_t iters = (total_keys + (uint64_t)size - 1) / (uint64_t)size;

    EVP_CIPHER_CTX *ctx_search = ctx_create(0, 0);
    unsigned char block[BLOCK_LEN];
    uint64_t local_found = UINT64_MAX;
    uint64_t global_found = UINT64_MAX;
    uint64_t local_tried = 0;

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    for (uint64_t it = 0; it < iters; it++) {
        const uint64_t key = it * (uint64_t)size + (uint64_t)rank;

        if (key < total_keys && local_found == UINT64_MAX) {
            local_tried++;
            crypt_buffer(ctx_search, key, iv, cipher, BLOCK_LEN, block);
            if (memcmp(block, known_block, BLOCK_LEN) == 0) {
                local_found = key;
            }
        }

        if ((it + 1) % CHECK_INTERVAL == 0) {
            MPI_Allreduce(&local_found, &global_found, 1, MPI_UINT64_T,
                          MPI_MIN, MPI_COMM_WORLD);
            if (global_found != UINT64_MAX) {
                break;
            }
        }
    }

    /* Reduccion final: cubre el caso de agotar el rango entre chequeos. */
    MPI_Allreduce(&local_found, &global_found, 1, MPI_UINT64_T,
                  MPI_MIN, MPI_COMM_WORLD);

    double elapsed = MPI_Wtime() - start;

    /* El tiempo de la ejecucion es el del proceso mas lento. */
    double max_elapsed = 0.0;
    uint64_t total_tried = 0;
    MPI_Reduce(&elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0,
               MPI_COMM_WORLD);
    MPI_Reduce(&local_tried, &total_tried, 1, MPI_UINT64_T, MPI_SUM, 0,
               MPI_COMM_WORLD);

    if (rank == 0) {
        if (global_found != UINT64_MAX) {
            /* Con la clave encontrada se descifra el mensaje completo. */
            EVP_CIPHER_CTX *ctx_dec = ctx_create(0, 1);
            unsigned char plain[MAX_MESSAGE_LEN + BLOCK_LEN];
            const int plain_len = crypt_buffer(
                ctx_dec, global_found, iv, cipher, cipher_len, plain);
            EVP_CIPHER_CTX_free(ctx_dec);

            if (plain_len < 0) {
                fail("La clave candidata no descifra el mensaje completo.");
            }

            unsigned char k[KEY_LEN];
            make_key(global_found, k);
            printf("Clave encontrada: %" PRIu64 "\n", global_found);
            printf("Clave (hex): ");
            print_hex(k, KEY_LEN);
            printf("Mensaje: ");
            fwrite(plain, 1, (size_t)plain_len, stdout);
            putchar('\n');
            printf("Coincide con el original: %s\n",
                   (plain_len == message_len &&
                    memcmp(plain, message, (size_t)message_len) == 0)
                       ? "si" : "no");
        } else {
            printf("No se encontro la clave en el rango.\n");
        }

        printf("Modo: AES-128-CBC, IV: ");
        print_hex(iv, BLOCK_LEN);
        printf("Candidatas probadas: %" PRIu64 " de %" PRIu64 "\n",
               total_tried, total_keys);
        printf("Rango explorado: 2^%" PRIu64 " de 2^128 claves (2^-%" PRIu64
               " del espacio)\n", bits, 128 - bits);
        if (max_elapsed > 0) {
            printf("Velocidad: %.0f claves/s\n",
                   (double)total_tried / max_elapsed);
        }
        printf("Ejecucion: MPI con %d procesos (distribucion ciclica)\n", size);
        printf("Tiempo: %.6f segundos\n", max_elapsed);
    }

    EVP_CIPHER_CTX_free(ctx_search);
    MPI_Finalize();
    return EXIT_SUCCESS;
}
