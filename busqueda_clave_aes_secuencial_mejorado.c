/*----------------------------------------------------------------------
 * UNIVERSIDAD DEL VALLE DE GUATEMALA
 * Curso:       CC3069 - Computacion Paralela y Distribuida
 * Ejercicio:   Busqueda secuencial de clave AES-128 (version corregida y mejorada)
 *
 * Mejoras respecto al programa original (ver INFORME_LAB03.md, seccion 3b):
 *   M1. Seguridad:    AES-128-CBC con IV aleatorio (RAND_bytes) en vez de ECB.
 *   M2. Flexibilidad: rango (bits), clave secreta y mensaje por linea de
 *                     comandos; mensajes de cualquier largo con relleno PKCS#7.
 *   M3. Rendimiento:  el contexto EVP (cifrador y relleno) se configura una
 *                     sola vez; por candidata solo se carga la clave.
 *   M4. Verificacion: la busqueda solo descifra y compara el PRIMER bloque
 *                     (fragmento conocido de 16 bytes), no el mensaje
 *                     completo; el mensaje entero se descifra una sola vez,
 *                     con la clave encontrada.
 *   M5. Reporte:      clave completa en hexadecimal, candidatas probadas,
 *                     claves por segundo y fraccion del espacio AES-128.
 *
 * Uso: ./busqueda_clave_aes_secuencial_mejorado [bits=20] [secreta=1000000]
 *                                              [mensaje="Puedes lograrlo!"]
 *----------------------------------------------------------------------*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <time.h>
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

/* Muestra el error y termina el programa. */
static void fail(const char *description)
{
    fprintf(stderr, "%s\n", description);
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}

/*
 * La candidata ocupa los 8 bytes bajos (big endian) y el resto es 0.
 * Se mantiene asi a proposito: el ejercicio necesita un espacio de
 * claves enumerable (ver seccion 3a del informe).
 */
static void make_key(uint64_t candidate, unsigned char key[KEY_LEN])
{
    memset(key, 0, KEY_LEN);
    for (int i = 0; i < 8; i++) {
        key[KEY_LEN - 1 - i] = (unsigned char)((candidate >> (8 * i)) & 0xFF);
    }
}

/* M3: configura el contexto una sola vez (AES-128-CBC y relleno). */
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

    /* NULL en el cifrador: se reutiliza la configuracion del contexto. */
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

/* Obtiene el tiempo de un reloj monotono, en segundos. */
static double get_time(void)
{
    struct timespec t;

    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        perror("Error al consultar el reloj");
        exit(EXIT_FAILURE);
    }
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

/* M2: convierte un argumento a entero sin signo, validando errores. */
static uint64_t parse_u64(const char *s, const char *name)
{
    char *end = NULL;

    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || s[0] == '-') {
        fprintf(stderr, "Argumento invalido para %s: '%s'\n", name, s);
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
    uint64_t bits = DEFAULT_BITS;
    uint64_t secret = DEFAULT_SECRET;
    const char *message = DEFAULT_MESSAGE;

    if (argc > 4) {
        fprintf(stderr, "Uso: %s [bits] [secreta] [mensaje]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 1) bits = parse_u64(argv[1], "bits");
    if (argc > 2) secret = parse_u64(argv[2], "secreta");
    if (argc > 3) message = argv[3];

    if (bits < 1 || bits > MAX_BITS) {
        fprintf(stderr, "bits debe estar entre 1 y %d.\n", MAX_BITS);
        return EXIT_FAILURE;
    }

    const int message_len = (int)strlen(message);
    if (message_len < 1 || message_len > MAX_MESSAGE_LEN) {
        fprintf(stderr, "El mensaje debe tener entre 1 y %d bytes.\n",
                MAX_MESSAGE_LEN);
        return EXIT_FAILURE;
    }

    const uint64_t total_keys = UINT64_C(1) << bits;
    if (secret >= total_keys) {
        printf("Aviso: la clave secreta esta fuera del rango 2^%" PRIu64
               "; se recorrera todo el rango sin encontrarla.\n", bits);
    }

    /* M1: IV aleatorio. Es publico y viaja junto al texto cifrado. */
    unsigned char iv[BLOCK_LEN];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        fail("No se pudo generar el IV.");
    }

    /* Cifrado del mensaje completo con relleno PKCS#7. */
    EVP_CIPHER_CTX *ctx_enc = ctx_create(1, 1);
    unsigned char cipher[MAX_MESSAGE_LEN + BLOCK_LEN];
    const int cipher_len = crypt_buffer(
        ctx_enc, secret, iv,
        (const unsigned char *)message, message_len, cipher);
    if (cipher_len < 0) {
        fail("Error al cifrar el mensaje.");
    }

    /*
     * M4: fragmento conocido = primer bloque del texto plano con relleno.
     * Modela un ataque de texto conocido (por ejemplo, la cabecera fija
     * de un archivo o protocolo) en lugar de conocer el mensaje entero.
     */
    unsigned char known_block[BLOCK_LEN];
    if (message_len >= BLOCK_LEN) {
        memcpy(known_block, message, BLOCK_LEN);
    } else {
        const unsigned char pad = (unsigned char)(BLOCK_LEN - message_len);
        memcpy(known_block, message, (size_t)message_len);
        memset(known_block + message_len, pad, BLOCK_LEN - (size_t)message_len);
    }

    /* Busqueda: un solo bloque por candidata y sin relleno. */
    EVP_CIPHER_CTX *ctx_search = ctx_create(0, 0);
    unsigned char block[BLOCK_LEN];
    uint64_t found = UINT64_MAX;
    uint64_t tried = 0;

    double start = get_time();

    for (uint64_t key = 0; key < total_keys; key++) {
        tried++;
        crypt_buffer(ctx_search, key, iv, cipher, BLOCK_LEN, block);
        if (memcmp(block, known_block, BLOCK_LEN) == 0) {
            found = key;
            break;
        }
    }

    double elapsed = get_time() - start;

    if (found != UINT64_MAX) {
        /* Con la clave encontrada se descifra el mensaje completo. */
        EVP_CIPHER_CTX *ctx_dec = ctx_create(0, 1);
        unsigned char plain[MAX_MESSAGE_LEN + BLOCK_LEN];
        const int plain_len = crypt_buffer(
            ctx_dec, found, iv, cipher, cipher_len, plain);
        EVP_CIPHER_CTX_free(ctx_dec);

        if (plain_len < 0) {
            fail("La clave candidata no descifra el mensaje completo.");
        }

        unsigned char k[KEY_LEN];
        make_key(found, k);
        printf("Clave encontrada: %" PRIu64 "\n", found);
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
           tried, total_keys);
    printf("Rango explorado: 2^%" PRIu64 " de 2^128 claves (2^-%" PRIu64
           " del espacio)\n", bits, 128 - bits);
    if (elapsed > 0) {
        printf("Velocidad: %.0f claves/s\n", (double)tried / elapsed);
    }
    printf("Ejecucion: secuencial\n");
    printf("Tiempo: %.6f segundos\n", elapsed);

    EVP_CIPHER_CTX_free(ctx_enc);
    EVP_CIPHER_CTX_free(ctx_search);
    return EXIT_SUCCESS;
}
