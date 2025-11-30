#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <limits.h>


//Parámetros del sistema
#define PAGE_SIZE          4096U          //4 KiB
#define PAGE_OFFSET_BITS   12             //log2(4096)
#define VIRTUAL_BITS       32
#define PAGE_NUMBER_BITS   (VIRTUAL_BITS - PAGE_OFFSET_BITS)

//Parámetros del TLB
#define TLB_MAX_ENTRIES    5
#define TLB_ENTRY_SIZE     32             //bytes por entrada (diseño propio)
#define TLB_SIZE_BYTES     (TLB_MAX_ENTRIES * TLB_ENTRY_SIZE) //<= 300

/*
    Layout de cada entrada dentro del bloque del TLB (sin struct, sólo offsets)
    Usamos enteros para representación decimal y binaria (la binaria se imprime
    con dec_to_bin).
    [0..3]   int           valid
    [4..7]   uint32_t      virt_addr
    [8..11]  uint32_t      page_dec
    [12..15] uint32_t      offset_dec
    [16..19] uint32_t      page_bin
    [20..23] uint32_t      offset_bin
    [24..31] unsigned long last_used   (para LRU)
 */
#define FIELD_VALID        0
#define FIELD_VADDR        4
#define FIELD_PAGE_DEC     8
#define FIELD_OFF_DEC      12
#define FIELD_PAGE_BIN     16
#define FIELD_OFF_BIN      20
#define FIELD_LAST_USED    24

// TLB en heap (segmento dinámico)
static unsigned char *tlb_base = NULL;

// VARIABLE_GLOBAL:Contador global para LRU
static unsigned long global_use_counter = 0;

// FUNCION: Conversión decimal -> binario usando >> y &                              
void dec_to_bin(unsigned long value, int bits, char *out_buffer)
{
    // out_buffer debe tener al menos bits+1 bytes, el +1 es para el carácter nulo (\0)
    for (int i = bits - 1; i >= 0; --i) { //se comienza desde la izquierda hacia la derecha
        unsigned long mask = 1UL << i; //aqui se crea una mascara donde 1 será estára en la i-esima posición
        // el resto serán 0s
        out_buffer[bits - 1 - i] = (value & mask) ? '1' : '0'; 
        //^_ cuando se almacena un valor (value) c sabe cual es su valor en binario
        // por tanto se hace la comparación si la mascara ([001]) y el valor de value ([101]) tienen 
        // un 1 en la misma posición, si es así entonces se almacena un 1 en la posición i-1 del buffer
    }
    out_buffer[bits] = '\0'; //se agrega el carácter nulo al final del string en el buffer
}

/*
    Función opcional binario -> decimal.
    Se calcula como suma de 2^posición para cada dígito '1', como pide el enunciado.
 */
unsigned long bin_to_dec(const char *bin_str)
{
    unsigned long result = 0;
    size_t len = strlen(bin_str);

    for (size_t i = 0; i < len; ++i) {
        char c = bin_str[len - 1 - i]; // posición i desde la derecha
        if (c == '1') {
            unsigned long power_of_two = (1UL << i);  //2^i
            result += power_of_two;
        }
    }
    return result;
}

//  Gestión del TLB (sólo punteros, sin arreglos ni structs)                
//  ---------------------------------------------------------

/*
    Inicializa el TLB en el heap.
    No se usan variables apuntador locales (sólo la global tlb_base).
 */
void tlb_init(void)
{
    tlb_base = (unsigned char *)malloc(TLB_SIZE_BYTES);
    if (!tlb_base) {
        fprintf(stderr, "Error: no se pudo reservar memoria para TLB\n");
        exit(EXIT_FAILURE);
    }

    /* Marcar todas las entradas como inválidas */
    unsigned char *entry = tlb_base;
    for (int i = 0; i < TLB_MAX_ENTRIES; ++i) {
        int *valid = (int *)(entry + FIELD_VALID);
        *valid = 0;
        entry += TLB_ENTRY_SIZE;
    }
}

/*
    Busca y/o actualiza el TLB con política LRU.
    Parámetros:
        -vaddr, page, offset: datos que se quieren traducir/insertar.
        -page_bin, offset_bin: mismas cantidades (las almacenamos también).
        -hit: (salida) 1 si TLB Hit, 0 si Miss.
        -replaced_address: (salida) dirección base de la entrada reemplazada o NULL.
        -use_seq: contador de uso creciente (para LRU).
    Restricción: máximo 3 variables apuntador locales en esta función.
 */

void tlb_lookup_and_update(uint32_t vaddr,
                           uint32_t page,
                           uint32_t offset,
                           uint32_t page_bin,
                           uint32_t offset_bin,
                           int *hit,
                           void **replaced_address,
                           unsigned long use_seq)
{
    unsigned char *entry = tlb_base;      /* 1er apuntador local */
    unsigned char *empty_entry = NULL;    /* 2do apuntador local */
    unsigned char *lru_entry = NULL;      /* 3er apuntador local */
    unsigned long lru_value = ULONG_MAX;  /* Valor inicial alto para LRU */

    /* Búsqueda en TLB y selección de LRU / entrada vacía */
    for (int i = 0; i < TLB_MAX_ENTRIES; ++i) {
        if (*((int *)(entry + FIELD_VALID))) {
            uint32_t *stored_vaddr = (uint32_t *)(entry + FIELD_VADDR);
            unsigned long *last_used = (unsigned long *)(entry + FIELD_LAST_USED);

            if (*stored_vaddr == vaddr) {
                /* TLB Hit */
                *hit = 1;
                *replaced_address = NULL;
                *last_used = use_seq; /* Actualizamos LRU */
                return;
            }

            /* Evaluamos si la entrada actual tiene el valor de last_used más bajo (LRU) */
            if (*last_used < lru_value) {
                lru_entry = entry;
                lru_value = *last_used;
            }
        } else {
            /* Encontramos una entrada vacía */
            if (empty_entry == NULL) {
                empty_entry = entry;
            }
        }
        entry += TLB_ENTRY_SIZE;
    }

    /* Si llegamos aquí, es Miss */
    *hit = 0;

    if (empty_entry != NULL) {
        /* Hay hueco libre: no hay reemplazo */
        entry = empty_entry;
        *replaced_address = NULL;
    } else {
        /* TLB lleno: reemplazamos la menos usada recientemente */
        entry = lru_entry;
        *replaced_address = (void *)entry;
    }

    /* Escritura de la nueva entrada con solo 3 punteros locales */
    *((int *)(entry + FIELD_VALID)) = 1;
    *((uint32_t *)(entry + FIELD_VADDR)) = vaddr;
    *((uint32_t *)(entry + FIELD_PAGE_DEC)) = page;
    *((uint32_t *)(entry + FIELD_OFF_DEC)) = offset;
    *((uint32_t *)(entry + FIELD_PAGE_BIN)) = page_bin;
    *((uint32_t *)(entry + FIELD_OFF_BIN)) = offset_bin;
    *((unsigned long *)(entry + FIELD_LAST_USED)) = use_seq;  /* Asignamos el contador de uso */
}


/* ------------------------------------------------------------------------- */
/* Función principal                                                         */
/* ------------------------------------------------------------------------- */

int main(void)
{
    char input_line[64];            /* buffer para leer la entrada */
    char *endptr;                   /* 1er apuntador local */
    void *replaced_address = NULL;  /* 2do apuntador local */

    struct timeval start_time, end_time;

    tlb_init(); /* reservar e inicializar TLB */

    /* Direcciones de inicio y fin del TLB */
    unsigned char *tlb_start = tlb_base;                     /* 3er apuntador local */
    unsigned char *tlb_end = tlb_base + TLB_SIZE_BYTES - 1;

    while (1) {
        printf("Ingrese dirección virtual: ");
        fflush(stdout);

        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            break; /* EOF o error */
        }

        /* Salir si el usuario escribe 's' (o 'S') como en el enunciado */
        if (input_line[0] == 's' || input_line[0] == 'S') {
            printf("Good bye!\n");
            break;
        }

        /* Medición del tiempo: inicio */
        if (gettimeofday(&start_time, NULL) != 0) {
            perror("gettimeofday");
            free(tlb_base);
            return EXIT_FAILURE;
        }

        /* Conversión de la entrada a entero sin signo */
        endptr = NULL;
        unsigned long long addr_ull = strtoull(input_line, &endptr, 10);

        /* Validación básica de entrada */
        if (endptr == input_line) {
            /* No se leyó ningún número válido */
            printf("Page Fault\n");
            continue;
        }

        /* Espacio virtual de 32 bits: [0, 2^32 - 1] */
        if (addr_ull > 0xFFFFFFFFULL) {
            printf("Page Fault\n");
            continue;
        }

        uint32_t vaddr = (uint32_t)addr_ull;

        /* Cálculo de número de página y desplazamiento */
        uint32_t page = vaddr >> PAGE_OFFSET_BITS;
        uint32_t offset = vaddr & (PAGE_SIZE - 1U);

        /* Valores binarios (guardamos la misma cantidad, se imprimen con dec_to_bin) */
        uint32_t page_bin_value = page;
        uint32_t offset_bin_value = offset;

        /* Contador global para LRU */
        global_use_counter++;

        int hit = 0;
        replaced_address = NULL;

        tlb_lookup_and_update(vaddr,
                              page,
                              offset,
                              page_bin_value,
                              offset_bin_value,
                              &hit,
                              &replaced_address,
                              global_use_counter);

        /* Medición del tiempo: fin */
        if (gettimeofday(&end_time, NULL) != 0) {
            perror("gettimeofday");
            free(tlb_base);
            return EXIT_FAILURE;
        }

        /* Cálculo del tiempo en segundos con parte decimal */
        double elapsed = (double)(end_time.tv_sec - start_time.tv_sec)
                         + (double)(end_time.tv_usec - start_time.tv_usec) / 1000000.0;

        /* Preparar representaciones binarias en texto */
        char page_bin_str[PAGE_NUMBER_BITS + 1];
        char offset_bin_str[PAGE_OFFSET_BITS + 1];

        dec_to_bin(page_bin_value, PAGE_NUMBER_BITS, page_bin_str);
        dec_to_bin(offset_bin_value, PAGE_OFFSET_BITS, offset_bin_str);

        /* Salida en el formato del ejemplo */
        printf("TLB desde %p hasta %p\n", (void *)tlb_start, (void *)tlb_end);

        if (hit) {
            printf("TLB Hit\n");
        } else {
            printf("TLB Miss\n");
        }

        printf("Página: %u\n", page);
        printf("Desplazamiento: %u\n", offset);
        printf("Página en binario: %s\n", page_bin_str);
        printf("Desplazamiento en binario: %s\n", offset_bin_str);

        /* Política de reemplazo */
        if (replaced_address == NULL) {
            printf("Politica de reemplazo: 0x0\n");
        } else {
            printf("--------------Politica de reemplazo: %p\n", replaced_address);
        }

        printf("Tiempo: %.6f segundos\n", elapsed);
        printf("\n");

    }
    free(tlb_base);
    return 0;
}
