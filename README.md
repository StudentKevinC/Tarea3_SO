# Tarea 3 - Sistemas Operativos

## Simulador de asignación de archivos

En esta tarea hice un programa en C que simula cómo se guardan archivos en un disco usando tres tipos de asignación: contigua, enlazada e indexada.

El disco se divide en bloques y el programa va creando archivos con tamaños aleatorios. También se pueden eliminar archivos y simular accesos a distintas posiciones para ver en qué bloque físico se encuentran.

Todo se hace en memoria, así que no se crean archivos reales en el computador.

## Compilación y ejecución

El programa está pensado para ejecutarse en un ambiente UNIX. En mi caso utilicé Ubuntu.

Primero hay que abrir una terminal dentro de la carpeta de la tarea y compilar con:

```bash
make
```

Después se ejecuta con:

```bash
./simulador
```

También se puede usar `make clean` para borrar el ejecutable y volver a compilarlo.

Al iniciar, el programa pide el tamaño del disco en MB, el tamaño de los bloques en KB, el mecanismo de asignación y el tamaño mínimo y máximo de los archivos.

Para elegir el mecanismo se ingresa 1 para contigua, 2 para enlazada o 3 para indexada.

Por ejemplo, una configuración que utilicé para las pruebas fue:

```text
Tamano del disco: 10
Tamano del bloque: 64
Asignacion: 2
Tamano minimo de archivo: 64
Tamano maximo de archivo: 128
```

## Cómo funciona

Cada 2 segundos se crea un archivo nuevo con un tamaño aleatorio dentro del rango que se ingresó al principio. Dependiendo del mecanismo elegido, el programa busca bloques libres y los asigna al archivo.

Después de los primeros 30 segundos comienzan las eliminaciones y los accesos. Cada 5 segundos se elimina un archivo elegido al azar y también se consulta una posición lógica de otro archivo.

Cuando se elimina un archivo, los bloques que ocupaba vuelven a quedar libres y pueden ser utilizados nuevamente.

En la terminal se van mostrando los archivos creados, sus bloques, las eliminaciones y los accesos realizados.

Si el programa no encuentra espacio suficiente para guardar un nuevo archivo, termina la simulación y muestra las estadísticas. También se puede detener manualmente con `Ctrl+C`.

## Tipos de asignación

**Contigua:** los bloques de cada archivo deben estar juntos. Para buscar un bloque basta con conocer dónde empieza el archivo y calcular su posición. El problema es que pueden quedar espacios libres separados y no alcanzar para guardar un archivo, aunque todavía exista espacio en el disco. Esto se conoce como fragmentación externa.

**Enlazada:** los bloques pueden estar en distintas partes del disco. Cada bloque reserva 4 bytes para guardar un puntero que indica cuál es el siguiente. Para encontrar un bloque hay que recorrer esos punteros desde el inicio del archivo, por lo que algunos accesos necesitan más pasos que otros.

**Indexada:** se utilizan bloques índice que guardan las direcciones de los bloques de datos. Primero se consulta el índice y después se llega al bloque que se necesita. En la simulación se consideran 2 accesos y también se reserva espacio para los bloques índice.

## Resultados de la simulación

Cuando termina el programa se muestra un resumen con el porcentaje de utilización del disco, los archivos creados y eliminados, los archivos que quedan guardados y la cantidad de bloques libres.

También aparecen los huecos disponibles, la fragmentación externa, la cantidad de consultas realizadas y el promedio de accesos necesarios para localizar los bloques.

En el caso de la asignación contigua, calculé la fragmentación externa contando los bloques libres que quedan fuera del hueco consecutivo más grande. Para enlazada e indexada se muestra cero, ya que no necesitan guardar los datos en bloques consecutivos.

## Archivos

El proyecto tiene estos archivos principales:

- `simulador.c`: contiene todo el código de la simulación.
- `Makefile`: sirve para compilar y borrar el ejecutable.
- `README.md`: contiene estas instrucciones y la explicación del programa.

También se debe entregar un video de máximo 12 minutos donde se muestre y explique el funcionamiento de los tres mecanismos, la creación y eliminación de archivos, los accesos y las estadísticas.