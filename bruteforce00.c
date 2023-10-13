#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <openssl/des.h>

void decrypt(const DES_cblock key, unsigned char *ciph, int len) {
    DES_key_schedule ks;
    DES_set_key_unchecked(&key, &ks);
    DES_ecb_encrypt((const_DES_cblock *)key, (DES_cblock *)ciph, &ks, DES_DECRYPT);
}

void encrypt(const DES_cblock key, unsigned char *ciph, int len) {
    DES_key_schedule ks;
    DES_set_key_unchecked(&key, &ks);
    DES_ecb_encrypt((const_DES_cblock *)key, (DES_cblock *)ciph, &ks, DES_ENCRYPT);
}

unsigned char cipher[] = {108, 245, 65, 63, 125, 200, 150, 66, 17, 170, 207, 170, 34, 31, 70, 215};

int main(int argc, char *argv[]) {
    int N, id;
    DES_cblock upper, mylower, myupper, found;
    MPI_Status st;
    MPI_Request req;
    int flag;
    int ciphlen = sizeof(cipher);
    MPI_Comm comm = MPI_COMM_WORLD;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(comm, &N);
    MPI_Comm_rank(comm, &id);

    memset(upper, 0, 8); // Inicializa la clave superior

    for (int i = 0; i < 6; i++) {
        upper[i] = 0x02; // Valor de la clave superior: 2^56
    }

    unsigned long long range_per_node = 1ULL << 56; // Tamaño total del espacio de claves

    for (int i = 0; i < 6; i++) {
        mylower[i] = (range_per_node / N) * id; // Clave inferior para este proceso
        myupper[i] = (range_per_node / N) * (id + 1) - 1; // Clave superior para este proceso
    }

    found[0] = 0; // Indicador de si se encontró la clave

    MPI_Irecv(found, 1, MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

    for (unsigned long long i = 0; i < range_per_node; ++i) {
        MPI_Test(&req, &flag, &st);
        if (flag) {
            break; // Otra instancia ya encontró la clave, salir
        }

        unsigned char temp[ciphlen];
        memcpy(temp, cipher, ciphlen);
        decrypt(mylower, temp, ciphlen);

        if (strstr(temp, "es una prueba de") != NULL) {
            found[0] = 1; // Indicar que encontramos la clave
            for (int node = 0; node < N; node++) {
                MPI_Send(found, 1, MPI_UNSIGNED_CHAR, node, 0, MPI_COMM_WORLD);
            }
            break;
        }

        // Avanza a la siguiente clave
        unsigned long long carry = 0;
        for (int j = 0; j < 6; j++) {
            unsigned long long sum = mylower[j] + carry;
            mylower[j] = (unsigned char)(sum & 0xFF);
            carry = sum >> 8;
            if (carry == 0) {
                break;
            }
        }
    }

    if (id == 0) {
        MPI_Wait(&req, &st);
        if (found[0] == 1) {
            printf("Clave encontrada\n");
        } else {
            printf("Clave no encontrada\n");
        }
    }

    MPI_Finalize();
}
