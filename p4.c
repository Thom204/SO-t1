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

sem_t *sw1, *sw2, *sr2,*rcsem; 
int *pbuffer = NULL; 
int shm_descriptor = -1;
int pipe2 = -1;

sem_t *semaphoreList[] = {NULL, NULL, NULL, NULL};
char *nameList[] = {"fib_sem", "pow_sem", "pdisplay_sem", "raceSem"};

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
                close(shm_descriptor);
                shm_unlink("shareBuff");
        }
        if(pbuffer != NULL || pbuffer != MAP_FAILED){
                munmap(pbuffer, sizeof(int));
        }
        if(pipe2 != -1){
                close(pipe2);
                unlink("/dev/shm/p2-p4_pipe");
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
        // codigo para enviar señal de interrupcion a p3 y p1.
        if(fork() == 0){
                execlp("bash", "bash", "-c", "pkill -x -SIGTERM p3; pkill -x -SIGTERM p1;", NULL);
                terminate();
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
        //por lo visto los buffer tienen que tener descriptores 
        //como si fueran archivos para poderse mmpaear.
        signal(SIGINT, handler);
        signal(SIGTERM, handler);

        sw2 = sem_open("pow_sem", O_CREAT | O_RDWR, 0666, 0);
        sr2 = sem_open("pdisplay_sem", O_CREAT | O_RDWR, 0666, 0);
        semaphoreList[1] = sw2;
        semaphoreList[2] = sr2;
 
        if(sem_wait(sw2) == -1){
                perror("wait sw2 failed");
                terminate();
                return -1;
        }


        shm_descriptor = shm_open("shareBuff", O_RDWR, 0666);   //todavía no se cual es el mode indicado, el ejemplo de man usa 0600.
        
        
        if (shm_descriptor == -1 || ftruncate(shm_descriptor, sizeof(int)) == -1) {
                perror("shm error"); 
                terminate();
                return -1;
        }
        pbuffer = (int *) mmap(NULL, sizeof(int), PROT_READ, MAP_SHARED, shm_descriptor, 0); 

        if (pbuffer == MAP_FAILED) {
                perror("mmap failed"); 
                terminate();
                return -1;
        }

        int content;
        int sval;
        sw1 = sem_open("fib_sem", 0);
        rcsem = sem_open("raceSem", 0);

        semaphoreList[0] = sw1;
        semaphoreList[3] = rcsem;

        if(verifySems() == -1){
                perror("error abriendo algun semaforo");
                terminate();
                return -1;
        }else{
                printf("esperando a p2\n");
        }

        //abrir tuberia.
        pipe2 = open("/dev/shm/p2-p4_pipe", O_WRONLY);
        if(pipe < 0){
                perror("error abriendo la tuberia");
                terminate();
                return -1;
        }

        //una vez creado el buffer se pone en espera.
        while(1){
                if(sem_wait(sr2) == -1){
                        perror("wait sr2 failed");
                        terminate();
                        return -1;
                }

                //leer contenido e imprimirlo.
                content = pbuffer[0];
                printf("%d\n", content);


                if (content == -2){
                        //enviar mensaje de salida a p2.
                        printf("p4 termina.");
                        char response[3] = "-3";

                        if(write(pipe2, response, sizeof(response)) == -1){
                                perror("write pipe failed");
                                terminate();
                                return -1;
                        }
                        break;
                }else {
                        if (sem_getvalue(rcsem, &sval) == -1){
                                perror("sem_getvalue failed");
                                terminate();
                                return -1;
                        }

                        if(sval == 0){
                                if(sem_post(rcsem)==-1){
                                        perror("post rcsem failed");
                                        terminate();
                                        return -1;
                                }  
                                //si hay alguien esperando a rcsem, 
                                //es el turno 1 y p2 inició, posteamos para 
                                //liberar a p1 de su wait rc. 
                        }

                        if(sem_post(sw1) == -1){
                                perror("post sw1 failed");
                                terminate();
                                return -1;
                        }
                }

        }
        clean_resources();
        return 0;
}
