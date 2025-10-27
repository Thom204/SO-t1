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

sem_t *sw1, *sr1, *sw2, *rcsem;
int *pbuffer = NULL;
int shm_descriptor = -1;
int shm_fd = -1;
int pipe1 = -1;

sem_t *semaphoreList[] = {NULL, NULL, NULL, NULL};
char *nameList[] = {"fib_sem", "fdisplay_sem", "pow_sem", "raceSem"};

void clean_resources(){
        for(int i = 0; i<4; i++){
                if(semaphoreList[i] == NULL){
                        continue;
                }else{
                        sem_close(semaphoreList[i]);
                        sem_unlink(nameList[i]);
                }
        }

        if(shm_descriptor != -1){
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
}

int verifySems(){
        for(int i = 0; i<4; i++){
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
        // codigo para enviar señal de interrupcion a p1 y p4.
        if(fork() == 0){
                execlp("bash", "bash", "-c", 
                       "pkill -x -SIGTERM p1; pkill -x -SIGTERM p4;", 
                       NULL);
        }else{
                wait(NULL);
        }
}


void handler(int sig){
	printf("señal recibida\n");
    if(sig == SIGINT){
            terminate();
            exit(1);
    }else if(sig == SIGTERM){
	        clean_resources();
	        exit(1);
    }
}

int main(int argc, char *argv[]){
        shm_descriptor = shm_open("shareBuff", O_CREAT | O_RDWR, 0666);   
        //todavía no se cual es el mode indicado, el ejemplo de man usa 0600.
        
        signal(SIGINT, handler);
        signal(SIGTERM, handler);
        
        if (shm_descriptor == -1 || 
            ftruncate(shm_descriptor, sizeof(int)) == -1) {

                perror("shm error");
                terminate();
                return -1;
        }

        pbuffer = (int *) mmap(NULL, sizeof(int), 
                   PROT_READ | PROT_WRITE, MAP_SHARED, 
                   shm_descriptor, 0); 

        if (pbuffer == MAP_FAILED){ 
                perror("mmap failed"); 
                terminate();
                return -1;
        }

        pbuffer[0]=0;
        int content;
        int sval;

        // al crear el semaforo del buffer, p1 hace dos post para avisar a 
        // p3 que abra el semaforo.
        sw1 = sem_open("fib_sem", O_CREAT | O_RDWR,0666,0);
        sr1 = sem_open("fdisplay_sem", O_CREAT | O_RDWR, 0666, 0);
        semaphoreList[0] = sw1;
        semaphoreList[1] = sr1;

        if(sem_wait(sw1) == -1){
                perror("wait sw1 failed");
                terminate();
                return -1;
        }

        sw2 = sem_open("pow_sem", 0);
        rcsem = sem_open("raceSem",0);
        semaphoreList[2] = sw2;
        semaphoreList[3] = rcsem;

        if(verifySems() == -1){
                perror("error abriendo algun semaforo");
                terminate();
                return -1;
        }else{
                printf("esperando a p1\n");
        } 

        pipe1 = open("/dev/shm/p1-p3_pipe", O_WRONLY);
        if(pipe < 0){
                perror("error abriendo la tuberia");
                terminate();
                return -1;
        }

        //una vez creado el buffer se pone en espera.
        while(1){
                if(sem_wait(sr1) == -1){
                        perror("wait sr1 failed");
                        terminate();
                        return -1;
                }
                
                //leer e imprimir.
                content = pbuffer[0];
                printf("%d\n", content);

                if (content == -1){
                        //enviar mensaje de salida a p1.
                        printf("p3 termina.");
                        char response[3] = "-3";
                        if(write(pipe1, response, sizeof(response)) == -1){
                                perror("write pipe failed");
                                terminate();
                                return -1;
                        }
                        break;
                }else {
                        
                        if(sem_getvalue(rcsem, &sval) == -1){
                                perror("sem_getvalue failed");
                                terminate();
                                return -1;
                        }

                        if (sval == 0){
                            if(sem_post(rcsem) == -1){
                                    perror("post rcsem failed");
                                    terminate();
                                    return -1;
                            }
                            //si hay alguien esperando al semaforo de rc, 
                            //(p1 inicio primero y es la primera ronda), 
                            //liberar  a p2 de su wait de race condition.
                        }
                        if(sem_post(sw2) == -1){
                                perror("post sw2 failed");
                                terminate();
                                return -1;
                        }
                }
        }
        clean_resources();
        return 0;
}
