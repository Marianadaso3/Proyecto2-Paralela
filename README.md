# Proyecto2-Paralela



## Ejecución del codigo

```shell
  mpicc <nomre_del_archivo>.c -o <ejecutable> -lssl  -lcrypto
  mpirun -np <numero_de_procesos> ./<archivo>  <clave>


  mpicc -o parteB parteB.c -lcrypto
  mpirun -np 4 ./parteB 12345678


```

## Miembros

1. Angel Higueros
2. Pablo Escobar
3. Mariana David
