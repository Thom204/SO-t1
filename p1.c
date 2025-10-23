#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <string.h>
#include <stdlib.h>
#define DBG 1

sem_t *sw1, *sr1, *sw2, *sr2, *rcsem;
int sval1, sval2;

// agregar err handling.
void fib(int* pbuffer,int a1, int a2, int N){
        int fibnum, ant, tmp;
        tmp = a2;
        ant = a1;
        fibnum = tmp + ant;
        
        for(int i = 0; i<N; i++){
                // esperar turno
                sem_wait(sw1);   //esperar turno de escritura

                // RC
                tmp = ant;
                ant = fibnum;
                fibnum += tmp;
                // escribir en buffer.


                if(DBG){
                        sleep(1);
                        pbuffer[0]++;
                }else{
                        pbuffer[0] = fibnum;
                }

                // postear para lectura del consumidor p3.
                sem_post(sr1);
        }
        sem_wait(sw1);
        pbuffer[0] = -1;
        sem_post(sr1);
}

void twopow(int *pbuffer, int a3, int N){
        int pow = 1;
        // calcular 2^a3
        for(int i = 0; i<a3; i++){ pow*=2;}

        for(int j = 0; j<N; j++){
                //esperar turno
                sem_wait(sw2);      //turno de escritura de productor

                // RC
                if(DBG){
                        sleep(1);
                        pbuffer[0]++;
                }else{
                        pbuffer[0] = pow;
                }
                pow *= 2;

                // postear mutex para lectura.
                sem_post(sr2);
        }
        sem_wait(sw2);
        pbuffer[0] = -2;
        sem_post(sr2);
}

int main(int argc, char *argv[]) {
        if (argc != 5) {
                printf("Uso: p1 a1 a2 a3 N.\n");
                return -1;
        }
        int a1 = atoi(argv[1]);
        int a2 = atoi(argv[2]);
        int a3 = atoi(argv[3]);
        int N = atoi(argv[4]);

        //validar existencia de p3 y p4
        //si p3 y p4 existen, fallara la creacion con sem open y solo abrira los valores ya existentes, por lo que no se inicializaran con 5 si no con 0.
        //en caso contrario se inicializaran con 5 y podremos identificar que no estan en ejecucion.

        sw1 = sem_open("fib_sem", O_CREAT | O_RDWR, 0666, 5);
        sw2 = sem_open("pow_sem", O_CREAT | O_RDWR, 0666, 5);
        //crear semaforo de mutex para el buffer.
        rcsem= sem_open("raceSem", O_CREAT, 0666, 1);

        //abrir recursos creados por p2 y p3.
        sr1 = sem_open("fdisplay_sem",0);
        sr2 = sem_open("pdisplay_sem",0);

        if (mkfifo("/dev/shm/p1-p3_pipe", 0666) == -1 || mkfifo("/dev/shm/p2-p4_pipe", 0666) == -1) {
                perror("pipe creation failure\n.");
                return -1;
        }

        // verificamos con sem_getvalue.
        sem_getvalue(sw1, &sval1);
        sem_getvalue(sw2, &sval2);

        if(sval1 > 4 || sval2 > 4){
                perror("p3. o p4.c no estan en ejecucion.");
                sem_close(sw1);
                sem_close(sw2);
                sem_close(sr1);
                sem_close(sr2);
                sem_close(rcsem);
                sem_unlink("fib_sem");
                sem_unlink("pow_sem");
                sem_unlink("raceSem");
                unlink("/dev/shm/p1-p3_pipe");
                unlink("/dev/shm/p2-p4_pipe");
                return -1;
               
        }else if(sval1 <= 0 && sval2 <= 0){
                printf("p3 y p4 listos y escuchando.%d %d\n", sval1, sval2);

                //librear a p3 y p4 del deadlock inicial.
                sem_post(sw1);
                sem_post(sw2);
        }

        int shm_fd = shm_open("shareBuff", O_RDWR, 0666);
        if (shm_fd == -1) {
                perror("shm open fail"); 
                return -1;
        }
        if(ftruncate(shm_fd, sizeof(int)) == -1) {
                perror("ftrunc err"); 
                return -1;
        }

        int *pbuffer = (int *) mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0);               
        if(pbuffer == MAP_FAILED){
                perror("map fail");
                return -1;
        }


        int rc = fork();
        if(rc < 0){
                perror("fork failed");
                return -1;
        }else if(rc==0){
                int pipe2 = open("/dev/shm/p2-p4_pipe", O_RDONLY);
                char exval2[3];
                sem_wait(rcsem);

                sem_getvalue(sw2, &sval2);
                
                if (DBG){
                        printf("hijo gana la race.\n");
                        printf("sw2 %d\n", sval2);
                }

                if(sval2 == 0){
                        sem_post(sw2);
                }

                twopow(pbuffer, a3, N);

                //listen para esperar el codigo de cierre.
                if(pipe2 < 0){
                        perror("no se pudo abrir la tuberia para proc 2.\n.");
                }else{
                        read(pipe2, exval2, sizeof(exval2));
                        if(atoi(exval2) == -3){
                            printf("termina p2.\n");
                        }
                        close(pipe2);
                }

        }else{
                int pipe1 = open("/dev/shm/p1-p3_pipe", O_RDONLY);
                char exval1[3];
                sem_wait(rcsem);
                
                sem_getvalue(sw1, &sval1);

                if(DBG){
                        printf("padre gana la race.\n");
                        printf("sw1 %d\n", sval1);
                }

                if(sval1 == 0){
                        sem_post(sw1);
                }

                fib(pbuffer, a1, a2, N);
                
                if(pipe1 < 0){
                        perror("no se pudo abrir la tuberia para proc 2.\n.");
                }else{
                        read(pipe1, exval1, sizeof(exval1));
                        if(atoi(exval1) == -3){
                            printf("termina p1.\n");
                        }
                        close(pipe1);
                }
        }
                munmap(pbuffer, sizeof(int));
                close(shm_fd);
                shm_unlink("shareBuff");
                sem_close(sw1);
                sem_close(sr1);
                sem_close(sw2);
                sem_close(sr2);
                sem_close(rcsem);
                sem_unlink("fib_sem");
                sem_unlink("pow_sem");
                sem_unlink("pdisplay_sem");
                sem_unlink("fdisplay_sem");
                sem_unlink("raceSem");
                unlink("/dev/shm/p1-p3_pipe");
                unlink("/dev/shm/p2-p4_pipe");
                
        return 0;
}
