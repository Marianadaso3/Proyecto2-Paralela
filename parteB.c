/**
 * @file parteB.c
 * @copyright Copyright (c) 2023
 *
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <openssl/des.h>

/**
 * @brief Read a file into a buffer
 * @param filename Nombre del archivo
 * @return char* Puntero al búfer del archivo leído
 */
char* readFile(const char* filename) {
    // Abrir el archivo para lectura en modo binario
    FILE* fp;
    char* buffer;
    long fileSize;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error al abrir el archivo");
        return NULL;
    }

    // Obtener el tamaño del archivo
    fseek(fp, 0L, SEEK_END);
    fileSize = ftell(fp);
    rewind(fp);

    // Asignar memoria para el búfer
    buffer = (char*) malloc(fileSize + 1);
    if (buffer == NULL) {
        fclose(fp);
        perror("Error al asignar memoria");
        return NULL;
    }

    // Leer el contenido del archivo en el búfer
    if (fread(buffer, fileSize, 1, fp) != 1) {
        fclose(fp);
        free(buffer);
        perror("Error al leer el archivo");
        return NULL;
    }

    // Agregar un carácter nulo al final del búfer
    buffer[fileSize] = '\0';

    // Cerrar el archivo y devolver el búfer
    fclose(fp);
    return buffer;
}

/**
 * @brief Descifra un texto dado una clave
 * @param key Clave para descifrar
 * @param ciph Texto a descifrar
 * @param len Longitud del texto
 *
 * @return void
 */


void decrypt(long key, char *ciph, int len) {
    
    // Establecer la paridad de la clave
    long k = 0;
    for (int i = 0; i < 8; ++i) {
        key <<= 1;
        k += (key & (0xFE << i * 8));
    }

    // Inicializar la clave DES
    DES_cblock desKey;
    memcpy(desKey, &k, sizeof(k));
    DES_set_odd_parity(&desKey);

    // Inicializar el programa de claves
    DES_key_schedule keySchedule;
    DES_set_key_unchecked(&desKey, &keySchedule);

    // Descifrar el mensaje usando el modo ECB
    for (size_t i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &keySchedule, DES_DECRYPT);
    }

    // Comprobar el relleno
    size_t padLen = ciph[len - 1];
    if (padLen > 8) {
        // Error: Relleno inválido
        return;
    }
    for (size_t i = len - padLen; i < len; i++) {
        if (ciph[i] != padLen) {
            // Error: Relleno inválido
            return;
        }
    }

    // Agregar el carácter nulo al texto plano
    ciph[len - padLen] = '\0';
}

/**
 * @brief Cifra un texto dado una clave
 * @param key Clave para cifrar
 * @param ciph Texto a cifrar
 * @param len Longitud del texto
 *
 * @return void
 */


void encryptText(long key, char *ciph, int len) {
    printf("--------Cifrando----------\n");
    // Establecer la paridad de la clave
    long k = 0;
    for (int i = 0; i < 8; ++i) {
        key <<= 1;
        k += (key & (0xFE << i * 8));
    }

    
    // Inicializar la clave DES
    DES_cblock desKey;
    memcpy(desKey, &k, sizeof(k));
    DES_set_odd_parity(&desKey);

    // Inicializar el programa de claves
    DES_key_schedule keySchedule;
    DES_set_key_unchecked(&desKey, &keySchedule);

    // Calcular la longitud del relleno
    size_t padLen = 8 - len % 8;
    if (padLen == 0) {
        padLen = 8;
    }

    // Agregar relleno al mensaje
    for (size_t i = len; i < len + padLen; i++) {
        ciph[i] = padLen;
    }

    // Cifrar el mensaje usando el modo ECB
    for (size_t i = 0; i < len + padLen; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &keySchedule, DES_ENCRYPT);
    }
}

// Palabra clave a buscar en el texto descifrado para determinar si el código fue roto
char search[] = "es una prueba de";

/**
 * @brief Intenta una clave para descifrar un texto
 * @param key Clave a intentar
 * @param ciph Texto a descifrar
 * @param len Longitud del texto
 *
 * @return int 1 si la clave es correcta, 0 en caso contrario
 */
int tryKey(long key, char *ciph, int len) {
    //printf("----------Intentando clave-----------\n");
    char temp[len+1]; // +1 debido al carácter nulo
    strcpy(temp, ciph);
    decrypt(key, temp, len);

    return strstr((char *)temp, search) != NULL;
}

long theKey = 3L;

int main(int argc, char *argv[]) { // char **argv
    if (argc < 2) {
        return 1;
    }
    theKey = strtol(argv[1], NULL, 10);

    int processSize, id;
    long upper = (1L << 56); // Límite superior de las claves DES (2^56)
    long myLower, myUpper;
    MPI_Status st;
    MPI_Request req;

    MPI_Comm comm = MPI_COMM_WORLD;
    double start, end;

    char* text = readFile("text.txt");
    if (text == NULL) {
        printf("Error al leer el archivo\n");
        return 0;
    }
    int ciphLen = strlen(text);

    // Cifrar el texto
    char cipher[ciphLen+1];
    memcpy(cipher, text, ciphLen);
    cipher[ciphLen]=0;

    encryptText(theKey, cipher, ciphLen);
    printf("Texto cifrado: %s\n", cipher);

    // Inicializar MPI
    MPI_Init(NULL, NULL);
    MPI_Comm_size(comm, &processSize);
    MPI_Comm_rank(comm, &id);

    long found = 0L;
    int ready = 0;

    // Distribución del trabajo
    long rangePerNode = upper / processSize;
    myLower = rangePerNode * id;
    myUpper = rangePerNode * (id + 1) - 1;
    if (id == processSize - 1) {
        myUpper = upper;
    }
    printf("Proceso %d rango inferior %ld rango superior %ld\n", id, myLower, myUpper);

    // Recepción no bloqueante, para verificar si otros procesos encontraron la clave
    start = MPI_Wtime();
    MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

    for (long i = myLower; i < myUpper; ++i) {
        MPI_Test(&req, &ready, MPI_STATUS_IGNORE);
        if (ready)
            break;  // Si alguien encontró la clave, detenerse

        if (tryKey(i, cipher, ciphLen)) {
            found = i;
            printf("Proceso %d encontró la clave\n", id);
            end = MPI_Wtime();
            for (int node = 0; node < processSize; node++) {
                MPI_Send(&found, 1, MPI_LONG, node, 0, comm); // Avisar a otros
            }
            break;
        }
    }

    // Esperar
    if (id == 0) {
        MPI_Wait(&req, &st);
        decrypt(found, cipher, ciphLen);
        printf("Clave = %li\n\n", found);
        cipher[ciphLen + 1] = '\0';
        printf("%s\n", cipher);
        printf("Tiempo para romper el DES: %f\n", end - start);
    }
    printf("Proceso %d saliendo\n", id);

    // Finalizar MPI
    MPI_Finalize();
}
