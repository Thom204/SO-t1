# SO-t1
## Setup:
* compilar el script flush.sh con: chmod u+x flush.sh
* este script se encarga de limpiar la memoria compartida cuando hay leaks.
## Orden de ejecucion:
* compilar todo con gcc (gcc -o p1 p1.c) para cada p#
* ejecutar ./p3 y ./p4, no importa el orden.
* ejecutar ./p1 con los parametros correctos.
### Notas:
* ejecutar p1 primero sin estar p3 ni p4 ya tiene el handle adecuado con semaforos, por lo cual no causa memory leaks.
* ejecutar p1 cuando solo esta uno de los demas procesos ejecutando causa un deadlock, resolver caso para cuando falta p3 o p4.
* en caso de salir nuevos errores o tener que forzar el cierre de un proceso, se debe ejecutar ./flush.sh para limpiar la memoria compartida.
* hay que asegurarse de que en ningun caso quede memoria sin liberar, se puede verificar si quedo memoria sin liberar con ./flush.sh viendo que la lista de aarchivos a eliminar sea nula.
* hay que verificar que todas las exigencias del planteamiento se cumplan.
