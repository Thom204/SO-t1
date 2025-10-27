#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

int DBG = 0;

sem_t *sw1, *sr1, *sw2, *sr2, *rcsem;

int sval1, sval2;
int *pbuffer = NULL;
int shm_fd = -1;
int pipe1 = -1;
int pipe2 = -1;

sem_t *semaphoreList[] = {NULL, NULL, NULL, NULL, NULL};

char *nameList[] = {"fib_sem", "fdisplay_sem", "pow_sem", 
                    "pdisplay_sem", "raceSem"};


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
        //el objetivo de esta funcion es que las interrupciones por errores
        //se propaguen entre procesos de modo que un error en cualquiera
        //genere la terminación de todos.
        //
        //podra ser un mecanismo overkill pero no me voy a arriesgar a que
        //algunos procesos queden en deadlock cuando otros terminan por errores
        //ya que esto evita que lso primeros puedan liberar sus recursos.

        clean_resources();
        // codigo para enviar señal de interrupcion a p3 y p4.
        if(fork() == 0){
                execlp("bash", "bash", "-c", 
                       "pkill -x -SIGTERM p3; pkill -x -SIGTERM p4;", 
                       NULL);
        }else{
                wait(NULL);
        }
}


void handler(int sig){
	    printf("Señal recibida\n");
        if(sig == SIGINT){
            //interrumpido por consola
            terminate();
	        exit(1);
        }else if(sig == SIGTERM){
            //interrumpido por otro proceso.
            clean_resources();
            exit(1);
        }
}


void fib(int* pbuffer,int a1, int a2, int N){
        int fibnum, ant, tmp;
        tmp = a2;
        ant = a1;
        fibnum = tmp + ant;
        
        for(int i = 0; i<N; i++){
            // esperar turno
    		// esperar turno de escritura
	        if(sem_wait(sw1) == -1){
			        perror("sem_wait(sw1) failed");
			        exit(1);
            }

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
            if(sem_post(sr1)==-1){
                    perror("post sr1 failed");
                    exit(1);
            }
        }
        if(sem_wait(sw1) == -1){
                perror("wait sw1 failed");
                exit(1);
        }
        pbuffer[0] = -1;
        if(sem_post(sr1) == -1){
                perror("post sr1 failed");
                exit(1);
        }
}


void twopow(int *pbuffer, int a3, int N){
        int pow = 1;
        // calcular 2^a3
        for(int i = 0; i<a3; i++){ pow*=2;}

        for(int j = 0; j<N; j++){
            //esperar turno
            if(sem_wait(sw2)== -1){    //turno de escritura de productor
                    perror("wait sw2 failed");
                    exit(1);
            }
            // RC
            if(DBG){
                    sleep(1);
                    pbuffer[0]++;
            }else{
                    pbuffer[0] = pow;
            }
            pow *= 2;

            if(sem_post(sr2) == -1){
                    perror("post sr2 failed");
                    exit(1);
            }
        }
        if(sem_wait(sw2) == -1){
                perror("wait sw2 failed");
                exit(1);
        }
        pbuffer[0] = -2;
        if(sem_post(sr2) == -1){
                perror("post sr2 failed");
                exit(1);
        }
}


int main(int argc, char *argv[]) {
        //añadido un modo debug que inmprime un buffer incremental
        //y tiene esperas entre los prints, el proposito de esto es
        //testear la sincronizacion.
        if(argc == 6){
                DBG = (atoi(argv[5])==1 ? 1 : 0);
        }else if (argc < 5) {
                printf("Uso: p1 a1 a2 a3 N.\n");
                return -1;
        }
        int a1 = atoi(argv[1]);
        int a2 = atoi(argv[2]);
        int a3 = atoi(argv[3]);
        int N = atoi(argv[4]);

	    //Implementación del handler 
        ////Termina el programa al presionar CTRL + C
	    signal(SIGINT, handler);
        //Termina el programa cuando recibe señales de los otros procesos
        signal(SIGTERM, handler);

        //validar existencia de p3 y p4
        //si p3 y p4 existen, fallara la creacion con sem open
        //y solo abrira los valores ya existentes, 
        //por lo que no se inicializaran con 5 si no con 0.
        //en caso contrario se inicializaran con 5 
        //y podremos identificar que no estan en ejecucion.
        sw1 = sem_open("fib_sem", O_CREAT | O_RDWR, 0666, 5);
        sw2 = sem_open("pow_sem", O_CREAT | O_RDWR, 0666, 5);
        rcsem= sem_open("raceSem", O_CREAT, 0666, 1);

        //abrir recursos creados por p2 y p3.
        sr1 = sem_open("fdisplay_sem",0);
        sr2 = sem_open("pdisplay_sem",0);

        semaphoreList[0]= sw1;
        semaphoreList[1]= sr1;
        semaphoreList[2]= sw2;
        semaphoreList[3]= sr2;
        semaphoreList[4]= rcsem;

        if(mkfifo("/dev/shm/p1-p3_pipe", 0666) == -1|| 
           mkfifo("/dev/shm/p2-p4_pipe", 0666) == -1) {
                perror("pipe creation failure\n.");
                terminate();
                return -1;
        }else{
                pipe1 = -2;
                pipe2 = -2;
        }

        // verificamos con sem_getvalue.
        if(sem_getvalue(sw1, &sval1) == -1 ||
           sem_getvalue(sw2, &sval2) == -1)  {
                perror("sem_getvalue error");
                terminate();
                return -1;
        }


        if(sval1 > 4){
		        printf("p3.c no está en ejecucción\n");
                terminate();
                return -1;

	    }   

        if(sval2 > 4){
                printf("p4.c no está en ejecucción\n");
                terminate();
                return -1;
	    }


        if(sval1 <= 0 && sval2 <= 0){
                printf("p3 y p4 listos y escuchando.\n");

                if(sem_post(sw1) == -1 || sem_post(sw2) == -1){
                        perror("post error");
                        terminate();
                        return -1;
                }
        }        

        if(verifySems() == -1){
                perror("error abriendo algun semaforo");
                terminate();
                return -1;
        }



        shm_fd = shm_open("shareBuff", O_RDWR, 0666);
        if (shm_fd == -1) {
                perror("shm open fail"); 
                terminate();
                return -1;
        }

        if(ftruncate(shm_fd, sizeof(int)) == -1) {
                perror("ftrunc err"); 
                return -1;
        }

        pbuffer = (int *) mmap(NULL, sizeof(int), 
                        PROT_WRITE | PROT_READ, MAP_SHARED, 
                        shm_fd, 0);

        if(pbuffer == MAP_FAILED){
                perror("map fail");
                terminate();
                return -1;
        }


        int rc = fork();
        if(rc < 0){
                perror("fork failed");
                terminate();
                return -1;
        }else if(rc==0){
                pipe2 = open("/dev/shm/p2-p4_pipe", O_RDONLY);
                char exval2[3];

                if(sem_wait(rcsem) == -1){
                        perror("wait error on rcsem");
                        terminate();
                        return -1;
                }

                if(sem_getvalue(sw2, &sval2) == -1){
                        perror("sem_getvalue failure for sw2");
                        terminate();
                        return -1;
                }
                
                if (DBG){
                        printf("hijo gana la race.\n");
                        printf("sw2 %d\n", sval2);
                }

                if(sval2 == 0){
                        if(sem_post(sw2) == -1){
                                perror("post sw2 failed");
                                terminate();
                                return -1;
                        }
                }

                twopow(pbuffer, a3, N);

                //listen para esperar el codigo de cierre.
                if(pipe2 < 0){
                        perror("no se pudo abrir la tuberia para proc 2.\n.");
                }else{
                        if(read(pipe2, exval2, sizeof(exval2)) == -1){
                                perror("pipe2 read failed");
                                terminate();
                                return -1;
                        }
                        if(atoi(exval2) == -3){
                                printf("%d termina p2.\n", atoi(exval2));
                        }
                        if(sem_post(sw1) == -1){
                                perror("post sw1 failed");
                                terminate();
                                return -1;
                        }
                        if(close(pipe2) == -1){
                                perror("close pipe2 failed");
                                terminate();
                                return -1;
                        }
                }

        }else{
                pipe1 = open("/dev/shm/p1-p3_pipe", O_RDONLY);
                char exval1[3];
                //sleep(1);    //sleep para comprobar race condition.
                if(sem_wait(rcsem) == -1){
                        perror("wait rcsem failed");
                        terminate();
                        return -1;
                }
                
                if(sem_getvalue(sw1, &sval1) == -1){
                        perror("sem_getvalue for sw1 failed");
                        terminate();
                        return -1;
                }

                if(DBG){
                        printf("padre gana la race.\n");
                        printf("sw1 %d\n", sval1);
                }

                if(sval1 == 0){
                        if(sem_post(sw1) == -1){
                                perror("post sw1 failed");
                                return -1;
                        }
                }

                fib(pbuffer, a1, a2, N);
                
                if(pipe1 < 0){
                        perror("no se pudo abrir la tuberia para proc 2.\n.");
                }else{
                        if(read(pipe1, exval1, sizeof(exval1)) == -1){
                                perror("read pipe1 failed");
                                terminate();
                                return -1;
                        }
                        if(atoi(exval1) == -3){
                                printf("%d termina p1.\n", atoi(exval1));
                        }
                        if(sem_post(sw2) == -1){
                                perror("post sw2 failed");
                                terminate();
                                return -1;
                        }
                        if(close(pipe1) == -1){
                                perror("colse pipe1 failed");
                                terminate();
                                return -1;
                        }
                }
                wait(NULL);     //wait para que los prints no queden feos.
        }
        //finalizacion normal.
        clean_resources(); 
        return 0;
}
