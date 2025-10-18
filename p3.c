#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <stdlib.h>

sem_t *sw1, *sr1, *sw2, *sr2, *buffermutex;

int main(int argc, char *argv[]){
        //por lo visto los buffer tienen que tener descriptores como si fueran archivos para poderse mmpaear.
        //shm_open, crea el descriptor que funciona como una especie de handler.

        int shm_descriptor = shm_open("shareBuff", O_CREAT | O_RDWR, 0666);   //todavía no se cual es el mode indicado, el ejemplo de man usa 0600.
        
        
        if (shm_descriptor == -1 || ftruncate(shm_descriptor, sizeof(int)) == -1) {
                perror("shm error"); 
                return 1;
        }

        int *pbuffer = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_descriptor, 0); 

        if (pbuffer == MAP_FAILED){ 
                perror("mmap failed"); 
                return 1;
        }
        pbuffer[0]=0;
        int content;
        sw1 = sem_open("fib_sem", 0);


        // al crear el semaforo del buffer, p1 hace dos post para avisar a p3 que abra el semaforo.
        sw1 = sem_open("fib_sem", O_CREAT | O_RDWR,0666,0);
        sr1 = sem_open("fdisplay_sem", O_CREAT | O_RDWR, 0666, 0);
        sem_wait(sw1);

        sw2 = sem_open("pow_sem", 0);
        buffermutex = sem_open("mutexSem", 0);
        int pipe = open("/dev/shm/p1-p3_pipe", O_WRONLY);
        if(buffermutex != SEM_FAILED && sw2 != SEM_FAILED && sw1 != SEM_FAILED && sr1 !=SEM_FAILED){
                printf("p3 armado y escuchando\n");
        }else{
                perror("error de armado de los semaforos");
                return -1;
        } 
        if(pipe < 0){
                perror("error abriendo la tuberia");
                return -1;
        }
        //una vez creado el buffer se pone en espera.
        while(1){
                sem_wait(sr1);
                //sem_wait(buffermutex);
                content = pbuffer[0];
                if (content == -1){
                        //enviar mensaje de salida a p1.
                        printf("p3 termina. %d", content);
                        char response[3] = "-3";
                        sem_post(buffermutex);
                        sem_post(sw2);
                        write(pipe, response, sizeof(response));
                        break;
                }else {
                        printf("%d\n", content);
                        sem_post(buffermutex);
                        sem_post(sw2);
                }
                // desbloquear el semaforo para las potencias.
        }
        munmap(pbuffer, sizeof(int));
        close(shm_descriptor);
        sem_close(sw1);
        sem_close(sr1);
        sem_close(sw2);
        sem_close(buffermutex);
        //close(pipe);
        return 0;
}
