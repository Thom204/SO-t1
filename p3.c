#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

sem_t *sw1, *sr1, *sw2, *sr2,*rcsem;
int *pbuffer = NULL;
int shm_descriptor = -1;
int shm_fd = -1;
int pipe1 = -1;
int pipe2 = -1;


sem_t *semaphoreList[] = {NULL, NULL, NULL, NULL, NULL};
char *nameList[] = {"fib_sem", "fdisplay_sem", "pow_sem", "pdisplay_sem", "raceSem"};

void clean_resources(){
        for(int i = 0; i<5; i++){
                if(semaphoreList[i] == NULL){
                        continue;
                }else{
                        sem_close(semaphoreList[i]);
                        sem_unlink(nameList[i]);
                }
        }

        if(shm_fd != -1){
                close(shm_fd);
                shm_unlink("shareBuff");
        }
        if(pbuffer != NULL || pbuffer != MAP_FAILED){
                munmap(pbuffer, sizeof(int));
        }
        if(pipe1 != -1){
                close(pipe1);
                unlink("/dev/shm/p1-p3_pipe");
        }
        if(pipe2 != -1){
                close(pipe2);
                unlink("/dev/shm/p2-p4_pipe");
        }
}

int verifySems(){
        for(int i = 0; i<5; i++){
                if(semaphoreList[i] == SEM_FAILED){
                        return -1;
                }else if(semaphoreList[i] == NULL){
                        return 0;
                }
        }
        return 0;
}

void terminate(){
        clean_resources();
        // codigo para enviar señal de interrupcion a p3 y p4.
        if(fork() == 0){
                execlp("bash", "bash", "-c", "pkill -x -SIGINT p3; pkill -x -SIGINT p4;", NULL);
        }else{
                wait(NULL);
        }
}


void handler(int sig){
	printf("señal receptada\n");
	clean_resources();
	_exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]){
        //por lo visto los buffer tienen que tener descriptores como si fueran archivos para poderse mmpaear.
        //shm_open, crea el descriptor que funciona como una especie de handler.

        int shm_descriptor = shm_open("shareBuff", O_CREAT | O_RDWR, 0666);   //todavía no se cual es el mode indicado, el ejemplo de man usa 0600.
        
        
        if (shm_descriptor == -1 || ftruncate(shm_descriptor, sizeof(int)) == -1) {
                perror("shm error"); 
                return 1;
        }

        pbuffer = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_descriptor, 0); 

        if (pbuffer == MAP_FAILED){ 
                perror("mmap failed"); 
                return 1;
        }
        pbuffer[0]=0;
        
        int content;
        int sval;

        // al crear el semaforo del buffer, p1 hace dos post para avisar a p3 que abra el semaforo.
        sw1 = sem_open("fib_sem", O_CREAT | O_RDWR,0666,0);
        sr1 = sem_open("fdisplay_sem", O_CREAT | O_RDWR, 0666, 0);
        sem_wait(sw1);

        sw2 = sem_open("pow_sem", 0);
        rcsem = sem_open("raceSem",0);
        int pipe = open("/dev/shm/p1-p3_pipe", O_WRONLY);
        if(sw2 != SEM_FAILED && sw1 != SEM_FAILED && sr1 !=SEM_FAILED){
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
                        //sem_post(sw2);
                        write(pipe, response, sizeof(response));
                        break;
                }else {
                        printf("%d\n", content);
                        
                        sem_getvalue(rcsem, &sval);
                        if (sval == 0){
                            sem_post(rcsem);  //si hay alguien esperando al semaforo de rc, (p1 inicio primero y es la primera ronda), liberar  a p2 de su wait de race condition.
                        }
                        sem_post(sw2);
                }
                // desbloquear el semaforo para las potencias.
        }
        clean_resources();
        //close(pipe);
        return 0;
}
