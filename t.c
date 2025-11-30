/* traducir.c*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>

/* RESTRICCIONES del TLB */
#define TLB_MAX_BYTES 300U
#define TLB_MAX_ENTRIES 5U

/* tamaños de cadenas binarias dentro de cada slot */
/* Ajuste: se aumentó OFF_BIN_SIZE de 13 a 15 para añadir 2 bytes de padding
   y así evitar accesos no alineados a uint32_t dentro del bloque. */
#define PAGE_BIN_SIZE 21U  /* 20 bits + '\0' */
#define OFF_BIN_SIZE  15U  /* 12 bits + '\0' + 2 bytes padding */

/* Offsets dentro del slot calculados de forma portable (parche aplicado) */
#define OFF_BASE     0U
#define OFF_PAGE     (OFF_BASE + (unsigned int)sizeof(uintptr_t))
#define OFF_OFFS     (OFF_PAGE + (unsigned int)sizeof(uint32_t))
/* page_bin comienza justo después de OFFS */
#define OFF_PAGE_BIN (OFF_OFFS)
#define OFF_OFF_BIN  (OFF_PAGE_BIN + PAGE_BIN_SIZE)
#define OFF_LRU      (OFF_OFF_BIN + OFF_BIN_SIZE)
#define SLOT_SIZE    (OFF_LRU + (unsigned int)sizeof(uint32_t))

/* Nota: la comprobación que estaba aquí usando #if con sizeof(...) fue
   removida porque el preprocesador no puede evaluar tokens como
   "(unsigned int)sizeof(...)" durante #if. En su lugar se realiza una
   comprobación simple en tiempo de ejecución dentro de init_tlb(). */

/* Variables globales del TLB en heap */
static char *tlb_heap = NULL;        /* puntero a inicio del TLB (heap) */
static const unsigned int tlb_bytes = TLB_MAX_BYTES;
static uint32_t access_counter = 1U; /* contador para LRU */

/* ---------- Funciones auxiliares (conversiones) ---------- */

/* decimal -> binario: usa >> y & tal como pide el enunciado.
   'bits' es el número de bits a producir, out debe tener espacio
   para bits + 1 (NUL). */
void dec_to_bin(uint32_t val, int bits, char *out)
{
    int i;
    int pos = 0;
    for (i = bits - 1; i >= 0; --i) {
        out[pos++] = ((val >> i) & 1U) ? '1' : '0';
    }
    out[pos] = '\0';
}

/* binario (cadena) -> decimal: suma acumulada de 2^posición */
uint32_t bin_to_dec(const char *bin)
{
    uint32_t res = 0U;
    size_t len = strlen(bin);
    size_t p;
    for (p = 0; p < len; ++p) {
        char c = bin[len - 1 - p];
        if (c == '1') {
            res += (1U << (unsigned int)p);
        }
    }
    return res;
}

/* Comprueba si el valor de addr (string) es válido para espacio 32-bit.
   Si válido, deja el valor en *out. */
int parse_address(const char *s, uint32_t *out)
{
    char *endptr = NULL;
    errno = 0;
    unsigned long val = strtoul(s, &endptr, 10);
    if (endptr == s || *endptr != '\0' || errno != 0) {
        return 0; /* no parseable */
    }
    if (val > UINT32_MAX) return 0;
    *out = (uint32_t)val;
    return 1;
}

/* ---------- Gestión del TLB (todas las funciones usan máximo 3
   variables apuntador char* cuando manipulan el TLB) ---------- */

/* Inicializa el TLB (malloc en heap) y marca entradas vacías.
   Esta función usa a lo sumo 3 punteros: base, cur, (tlb_heap global no cuenta). */
void init_tlb(void)
{
    tlb_heap = (char *)malloc((size_t)tlb_bytes);
    if (!tlb_heap) {
        perror("malloc TLB");
        exit(EXIT_FAILURE);
    }

    /* Comprobación en tiempo de ejecución del tamaño total */
    if ((SLOT_SIZE * TLB_MAX_ENTRIES) > TLB_MAX_BYTES) {
        fprintf(stderr, "warning: SLOT_SIZE * TLB_MAX_ENTRIES excede TLB_MAX_BYTES; ajustar tamaños\n");
    }

    /* marcar entradas como vacías: page = UINT32_MAX */
    char *base = tlb_heap;
    char *cur;
    unsigned int i;
    for (i = 0U; i < TLB_MAX_ENTRIES; ++i) {
        cur = base + (size_t)i * (size_t)SLOT_SIZE;
        /* page = UINT32_MAX indica slot vacío */
        *((uint32_t *)(cur + OFF_PAGE)) = UINT32_MAX;
        /* lru = 0 */
        *((uint32_t *)(cur + OFF_LRU)) = 0U;
        /* base pointer = 0 */
        *((uintptr_t *)(cur + OFF_BASE)) = (uintptr_t)0;
        /* limpiar cadenas binarias por claridad (opcional) */
        memset(cur + OFF_PAGE_BIN, 0, PAGE_BIN_SIZE);
        memset(cur + OFF_OFF_BIN, 0, OFF_BIN_SIZE);
    }
}

/* Libera recursos del TLB */
void free_tlb(void)
{
    if (tlb_heap) {
        free(tlb_heap);
        tlb_heap = NULL;
    }
}

/* Busca en el TLB la entrada cuya page == page_num.
   Devuelve puntero al slot (char*) o NULL.
   Usa ≤3 punteros: start, cur, found. */
char *tlb_find(uint32_t page_num)
{
    char *start = tlb_heap;
    char *cur = NULL;
    char *found = NULL;
    unsigned int i;
    for (i = 0U; i < TLB_MAX_ENTRIES; ++i) {
        cur = start + (size_t)i * (size_t)SLOT_SIZE;
        uint32_t p = *((uint32_t *)(cur + OFF_PAGE));
        if (p == page_num) { found = cur; break; }
    }
    return found;
}

/* Inserta/actualiza una entrada en el TLB (LRU).
   Devuelve la dirección base de memoria (uintptr_t) que fue reemplazada,
   o (uintptr_t)0 si no hubo reemplazo (inserción en slot libre).
   Usa ≤3 punteros: start, cur, victim. */
uintptr_t tlb_insert(uint32_t page_num,
                     uint32_t offset_num,
                     const char *page_bin, const char *off_bin)
{
    char *start = tlb_heap;
    char *cur = NULL;
    char *victim = NULL; /* se usa también como candidato LRU */
    unsigned int i;

    /* 1) Buscar slot vacío */
    for (i = 0U; i < TLB_MAX_ENTRIES; ++i) {
        cur = start + (size_t)i * (size_t)SLOT_SIZE;
        uint32_t p = *((uint32_t *)(cur + OFF_PAGE));
        if (p == UINT32_MAX) {
            /* slot vacío -> insertar aquí (guardamos la dirección del slot) */
            *((uintptr_t *)(cur + OFF_BASE)) = (uintptr_t)cur;
            *((uint32_t *)(cur + OFF_PAGE)) = page_num;
            *((uint32_t *)(cur + OFF_OFFS)) = offset_num;
            memcpy(cur + OFF_PAGE_BIN, page_bin, PAGE_BIN_SIZE);
            memcpy(cur + OFF_OFF_BIN, off_bin, OFF_BIN_SIZE);
            *((uint32_t *)(cur + OFF_LRU)) = access_counter++;
            return (uintptr_t)0; /* no hubo reemplazo */
        }
    }

    /* 2) TLB lleno -> buscar victim por LRU (min) usando 'victim' */
    uint32_t min_lru = UINT32_MAX;
    for (i = 0U; i < TLB_MAX_ENTRIES; ++i) {
        cur = start + (size_t)i * (size_t)SLOT_SIZE;
        uint32_t l = *((uint32_t *)(cur + OFF_LRU));
        if (l < min_lru) { min_lru = l; victim = cur; }
    }

    if (victim) {
        uintptr_t replaced_base = *((uintptr_t *)(victim + OFF_BASE));
        /* reemplazar contenido de victim */
        *((uintptr_t *)(victim + OFF_BASE)) = (uintptr_t)victim;
        *((uint32_t *)(victim + OFF_PAGE)) = page_num;
        *((uint32_t *)(victim + OFF_OFFS)) = offset_num;
        memcpy(victim + OFF_PAGE_BIN, page_bin, PAGE_BIN_SIZE);
        memcpy(victim + OFF_OFF_BIN, off_bin, OFF_BIN_SIZE);
        *((uint32_t *)(victim + OFF_LRU)) = access_counter++;
        return replaced_base;
    }
    return (uintptr_t)0;
}

/* Actualiza el LRU de la entrada al valor actual de access_counter.
   Usa ≤3 punteros: (no declara más punteros aquí) */
void tlb_update_lru(char *slot_ptr)
{
    *((uint32_t *)(slot_ptr + OFF_LRU)) = access_counter++;
}

/* Muestra la dirección base-fín del TLB como pide el enunciado */
void print_tlb_bounds(void)
{
    printf("TLB desde %p hasta %p\n",
           (void *)tlb_heap,
           (void *)(tlb_heap + (size_t)tlb_bytes - 1U));
}

/* ---------- Programa principal ---------- */

int main(void)
{
    char line[128];
    init_tlb(); /* crea region en heap y marca vacío */

    while (1) {
        printf("Ingrese dirección virtual: ");
        if (!fgets(line, sizeof(line), stdin)) break;
        /* quitar '\n' */
        size_t ln = strlen(line);
        if (ln > 0 && line[ln - 1] == '\n') line[ln - 1] = '\0';
        if (strcmp(line, "s") == 0) {
            printf("Good bye!\n");
            break;
        }

        /* imprimir bounds (requisito) */
        print_tlb_bounds();

        /* parseo y validación */
        uint32_t vaddr;
        if (!parse_address(line, &vaddr)) {
            printf("Page Fault\n");
            continue;
        }

        /* calcular página y offset */
        uint32_t page_num = vaddr >> 12;         /* 20 bits */
        uint32_t offset_num = vaddr & 0xFFFU;    /* 12 bits */

        /* preparar representaciones binarias (variables requeridas) */
        char page_bin[PAGE_BIN_SIZE]; /* variable para pagina en binario */
        char off_bin[OFF_BIN_SIZE];   /* variable para offset en binario */
        dec_to_bin(page_num, 20, page_bin);
        dec_to_bin(offset_num, 12, off_bin);

        /* medir tiempo: inicio */
        struct timeval t0, t1;
        gettimeofday(&t0, NULL);

        /* buscar en TLB */
        char *slot = tlb_find(page_num);
        if (slot) {
            /* HIT */
            printf("TLB Hit\n");
            tlb_update_lru(slot);
            /* al ser hit, no hubo reemplazo -> mostrar 0x0 */
            printf("Politica de reemplazo: 0x0\n");
        } else {
            /* MISS: insertar y posiblemente reemplazar */
            printf("TLB Miss\n");
            uintptr_t replaced = tlb_insert(page_num, offset_num,
                                            page_bin, off_bin);
            if (replaced == (uintptr_t)0) {
                printf("Politica de reemplazo: 0x0\n");
            } else {
                printf("Politica de reemplazo: %p\n", (void *)replaced);
            }
        }

        /* medir tiempo: fin */
        gettimeofday(&t1, NULL);
        double elapsed = (t1.tv_sec - t0.tv_sec) +
            (t1.tv_usec - t0.tv_usec) / 1e6;

        /* Mostrar resultados (formato similar al ejemplo) */
        printf("Página: %u\n", page_num);
        printf("Desplazamiento: %u\n", offset_num);
        printf("Página en binario: %s\n", page_bin);
        printf("Desplazamiento en binario: %s\n", off_bin);
        printf("Tiempo: %.6f segundos\n\n", elapsed);
    }

    free_tlb();
    return 0;
}
