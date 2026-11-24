
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define PUNTERO 4

typedef struct {
    int id;
    long long bytes;
    int datos, total;
    int inicio;
    int *bloques;
    int *indices;
    int cant_indices;
    int *tabla_indice;
} Archivo;


static unsigned char *disco;
static int32_t *enlaces;

static int cantidad_bloques, bloque_bytes, mecanismo;
static long long minimo_bytes, maximo_bytes;

static Archivo **archivos;
static int cantidad_archivos = 0, capacidad_archivos = 0;

static int siguiente_id = 1, creados = 0, eliminados = 0;
static long long accesos_total = 0, consultas = 0;

static volatile sig_atomic_t detener = 0;
static unsigned int semilla;


static void senal(int s) {
    (void)s;
    detener = 1;
}


static void esperar_ms(int ms) {
    struct timespec t = {ms / 1000, (ms % 1000) * 1000000L};
    while (nanosleep(&t, &t) == -1 && errno == EINTR && !detener) {
    }
}


static unsigned int aleatorio(void) {
    semilla = semilla * 1664525u + 1013904223u;

    return semilla;
}


static long long azar_rango(long long a, long long b) {
    unsigned long long rango = (unsigned long long)(b - a) + 1ULL;
    unsigned long long valor = ((unsigned long long)aleatorio() << 32) | aleatorio();

    return a + (long long)(valor % rango);
}

// cuantos bloques quedan

static int libres(void) {
    int n = 0;
    for (int i = 0; i < cantidad_bloques; i++) {
        if (!disco[i]) n++;
    }

    return n;
}

// buscamos el hueco mas grande

static int mayor_hueco(void) {
    int actual = 0, mayor = 0;
    for (int i = 0; i < cantidad_bloques; i++) {
        if (!disco[i]) {
            actual++;
            if (actual > mayor) mayor = actual;
        }
        else actual = 0;
    }

    return mayor;
}

// cantidad de huecos libres

static int huecos(void) {
    int n = 0, anterior_ocupado = 1;
    for (int i = 0; i < cantidad_bloques; i++) {
        if (!disco[i] && anterior_ocupado) n++;
        anterior_ocupado = disco[i] != 0;
    }

    return n;
}

// fragmentacion para el caso contiguo

static int fragmentacion_externa(void) {
    if (mecanismo != 1) return 0;

    return libres() - mayor_hueco();
}


static void estado(void) {
    printf("Disco: %d/%d bloques ocupados | libres: %d | huecos: %d | fragmentacion externa: %d bloques\n",
           cantidad_bloques - libres(), cantidad_bloques, libres(),
           huecos(), fragmentacion_externa());
}

// esto se usa en contigua

static int buscar_contiguos(int n) {
    int seguidos = 0;
    for (int i = 0; i < cantidad_bloques; i++) {
        if (!disco[i]) seguidos++;
        else seguidos = 0;
        if (seguidos == n) return i - n + 1;
    }

    return -1;
}

// libera los bloques cuando borramos un archivo

static void liberar_archivo(Archivo *a) {
    if (!a) return;
    for (int i = 0; i < a->total; i++) {
        int b = a->bloques[i];
        disco[b] = 0;
        enlaces[b] = -1;
    }
    free(a->bloques);
    free(a->indices);
    free(a->tabla_indice);
    free(a);
}


static int guardar_archivo(Archivo *a) {
    if (cantidad_archivos == capacidad_archivos) {
        int nueva = capacidad_archivos ? capacidad_archivos * 2 : 16;
        Archivo **p = realloc(archivos, (size_t)nueva * sizeof(*archivos));
        if (!p) return 0;
        archivos = p;
        capacidad_archivos = nueva;
    }
    archivos[cantidad_archivos++] = a;

    return 1;
}

// creamos un archivo en el disco simulado
// 1 sale bien, 0 no cabe y -1 es error

static int crear_archivo(void) {
    long long tamano = azar_rango(minimo_bytes, maximo_bytes);
    int utiles = bloque_bytes - (mecanismo == 2 ? PUNTERO : 0);
    int datos = (int)((tamano + utiles - 1) / utiles);
    int indices = 0;
    if (mecanismo == 3) {
        int por_indice = bloque_bytes / PUNTERO;
        indices = (datos + por_indice - 1) / por_indice;
    }
    int total = datos + indices;
    if (total > libres()) {
        printf("\nNo hay espacio suficiente para un archivo de %lld KB (%d bloques requeridos).\n",
               (tamano + 1023) / 1024, total);

        return 0;
    }
    int inicio = -1;
    if (mecanismo == 1) {
        inicio = buscar_contiguos(total);
        if (inicio < 0) {

            printf("\nFragmentacion externa: hay %d bloques libres, pero no %d consecutivos.\n",
                   libres(), total);

            return 0;
        }
    }
    Archivo *a = calloc(1, sizeof(*a));
    if (!a) return -1;
    a->bloques = malloc((size_t)total * sizeof(int));
    if (indices) {
        a->indices = malloc((size_t)indices * sizeof(int));
        a->tabla_indice = malloc((size_t)datos * sizeof(int));
    }
    if (!a->bloques || (indices && (!a->indices || !a->tabla_indice))) {
        free(a->bloques);
        free(a->indices);
        free(a->tabla_indice);
        free(a);

        return -1;
    }
    a->id = siguiente_id++;
    a->bytes = tamano;
    a->datos = datos;
    a->total = total;
    a->cant_indices = indices;
    int puestos = 0;
    for (int i = 0; i < cantidad_bloques && puestos < total; i++) {
        if ((mecanismo == 1 && i >= inicio && i < inicio + total) ||
            (mecanismo != 1 && !disco[i])) {
            a->bloques[puestos++] = i;
            disco[i] = 1;
        }
    }
    a->inicio = a->bloques[0];
    if (mecanismo == 2) {
        // cada bloque apunta al siguiente
        for (int i = 0; i < datos; i++) {
            int actual = a->bloques[i];
            enlaces[actual] = (i + 1 < datos) ? a->bloques[i + 1] : -1;
        }
    }
    if (mecanismo == 3) {
        // guardamos las posiciones en el indice
        for (int i = 0; i < indices; i++)
            a->indices[i] = a->bloques[datos + i];
        for (int i = 0; i < datos; i++)
            a->tabla_indice[i] = a->bloques[i];
    }
    if (!guardar_archivo(a)) {
        liberar_archivo(a);

        return -1;
    }

    creados++;
    printf("\n[CREAR] Archivo %d | %lld bytes | datos: %d bloques | indices: %d | ubicacion:",
           a->id, a->bytes, datos, indices);
    for (int i = 0; i < total; i++)
        printf(" %d", a->bloques[i]);

    printf("\n");

    estado();

    return 1;
}

// borramos uno al azar

static void eliminar_aleatorio(void) {
    if (!cantidad_archivos) {
        puts("[ELIMINAR] No hay archivos para eliminar.");
        return;
    }
    int elegido = (int)(aleatorio() % (unsigned int)cantidad_archivos);
    Archivo *a = archivos[elegido];

    printf("\n[ELIMINAR] Archivo %d | libera %d bloques\n",
           a->id, a->total);
    liberar_archivo(a);
    archivos[elegido] = archivos[cantidad_archivos - 1];
    cantidad_archivos--;

    eliminados++;

    estado();
}

// aca se busca el bloque que pidio el acceso

static void acceder_aleatorio(void) {
    if (!cantidad_archivos) {
        puts("[ACCESO] No hay archivos para consultar.");
        return;
    }
    Archivo *a = archivos[aleatorio() % (unsigned int)cantidad_archivos];
    long long posicion = azar_rango(0, a->bytes - 1);
    int utiles = bloque_bytes - (mecanismo == 2 ? PUNTERO : 0);
    int bloque_logico = (int)(posicion / utiles);
    int desplazamiento = (int)(posicion % utiles);
    int fisico = -1, accesos = 0, indice_fisico = -1;
    if (mecanismo == 1) {
        // en contigua es una suma
        fisico = a->inicio + bloque_logico;
        accesos = 1;
    }
    else if (mecanismo == 2) {
        // aca toca seguir los punteros
        int actual = a->inicio;
        for (int i = 0; i <= bloque_logico && actual != -1; i++) {
            fisico = actual;
            accesos++;
            if (i < bloque_logico)
                actual = enlaces[actual];
        }
    }
    else {
        // primero el indice y luego los datos
        int por_indice = bloque_bytes / PUNTERO;
        int numero_indice = bloque_logico / por_indice;
        indice_fisico = a->indices[numero_indice];
        fisico = a->tabla_indice[bloque_logico];
        accesos = 2;
    }
    printf("\n[ACCESO] Archivo %d | byte logico %lld | bloque logico %d | bloque fisico %d | desplazamiento %d | accesos: %d",
           a->id, posicion, bloque_logico, fisico, desplazamiento, accesos);
    if (mecanismo == 3)
        printf(" | bloque indice %d", indice_fisico);
    printf("\n");
    accesos_total += accesos;

    consultas++;
}

// resumen de la simulacion

static void estadisticas(void) {
    int ocupados = cantidad_bloques - libres();
    puts("\n========== ESTADISTICAS FINALES ==========");
    printf("Mecanismo: %s\n",
           mecanismo == 1 ? "contigua" :
           mecanismo == 2 ? "enlazada" : "indexada");
    printf("Uso del disco: %d de %d bloques (%.2f%%)\n",
           ocupados, cantidad_bloques,
           100.0 * ocupados / cantidad_bloques);
    printf("Archivos creados: %d\n", creados);
    printf("Archivos eliminados: %d\n", eliminados);

    printf("Archivos restantes: %d\n", cantidad_archivos);
    printf("Bloques libres: %d | huecos libres: %d\n",
           libres(), huecos()) ;

    printf("Fragmentacion externa: %d bloques (%.2f KB)\n",
           fragmentacion_externa(),
           (double)fragmentacion_externa() * bloque_bytes / 1024);

    printf("Consultas realizadas: %lld\n", consultas);
    printf("Promedio de accesos: %.2f\n",
           consultas ? (double)accesos_total / consultas : 0.0);
    puts("=========================================");
}


static int pedir_entero(const char *mensaje, int min, int max) {
    char linea[128], extra;
    int valor;
    while (1) {
        printf("%s", mensaje);
        fflush(stdout);
        if (!fgets(linea, sizeof(linea), stdin))

            return -1;
        if (sscanf(linea, " %d %c", &valor, &extra) == 1 &&
            valor >= min && valor <= max)

            return valor;
        printf("Ingresa un numero entre %d y %d.\n", min, max);
    }
}


int main(void) {
    signal(SIGINT, senal);
    semilla = (unsigned int)time(NULL);
    puts("=== SIMULADOR DE ASIGNACION DE ARCHIVOS ===");
    int mb = pedir_entero("Tamano del disco (MB, 1 a 1024): ", 1, 1024);
    if (mb < 0) return 1;
    int kb = pedir_entero("Tamano del bloque (KB, 1 a 1024): ", 1, 1024);
    if (kb < 0) return 1;
    mecanismo = pedir_entero("Asignacion (1 contigua, 2 enlazada, 3 indexada): ", 1, 3);
    if (mecanismo < 0) return 1;
    int min_kb = pedir_entero("Tamano minimo de archivo (KB, 1 a 102400): ", 1, 102400);
    if (min_kb < 0) return 1;
    int max_kb = pedir_entero("Tamano maximo de archivo (KB): ", min_kb, 102400);
    if (max_kb < 0) return 1;
    bloque_bytes = kb * 1024;
    cantidad_bloques = (int)(((long long)mb * 1024) / kb);
    minimo_bytes = (long long)min_kb * 1024;
    maximo_bytes = (long long)max_kb * 1024;
    disco = calloc((size_t)cantidad_bloques, 1);
    enlaces = malloc((size_t)cantidad_bloques * sizeof(*enlaces));
    if (!disco || !enlaces) {
        puts("Error reservando memoria para el disco.");
        free(disco);
        free(enlaces);

        return 1;
    }
    for (int i = 0; i < cantidad_bloques; i++)
        enlaces[i] = -1;

    printf("\nDisco listo: %d bloques de %d KB. Ctrl+C para terminar.\n",
           cantidad_bloques, kb);
    puts("Creacion cada 2 segundos. Desde los 30 segundos: eliminacion y acceso cada 5 segundos.\n");

    estado();
    struct timespec inicio, ahora;
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    long long proxima_creacion = 2000;
    long long proxima_eliminacion = 30000;
    long long proximo_acceso = 30000;
    int fallo = 0;
    while (!detener) {
        clock_gettime(CLOCK_MONOTONIC, &ahora);
        long long transcurrido =
            (ahora.tv_sec - inicio.tv_sec) * 1000LL +
            (ahora.tv_nsec - inicio.tv_nsec) / 1000000LL;
        if (transcurrido >= proxima_creacion) {
            int resultado = crear_archivo();
            proxima_creacion += 2000;
            if (resultado != 1) {
                fallo = resultado;
                break;
            }
        }
        if (transcurrido >= proxima_eliminacion) {
            eliminar_aleatorio();
            proxima_eliminacion += 5000;
        }
        if (transcurrido >= proximo_acceso) {
            acceder_aleatorio();
            proximo_acceso += 5000;
        }
        if (!detener)
            esperar_ms(50);
    }
    if (detener)
        puts("\nSimulacion detenida con Ctrl+C.");
    else if (fallo < 0)
        puts("\nSimulacion detenida por error de memoria.");
    else
        puts("\nSimulacion terminada: no se pudo asignar un archivo.");

    estadisticas();
    for (int i = 0; i < cantidad_archivos; i++)
        liberar_archivo(archivos[i]);
    free(archivos);
    free(enlaces);
    free(disco);

    return fallo < 0 ? 1 : 0;
}
