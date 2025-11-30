#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <limits.h>

#define PAGE_SIZE 4096U
#define PAGE_OFFSET_BITS 12
#define VIRTUAL_BITS 32
#define PAGE_NUMBER_BITS (VIRTUAL_BITS - PAGE_OFFSET_BITS)

#define TLB_MAX_ENTRIES 5
#define TLB_ENTRY_SIZE 32
#define TLB_SIZE_BYTES (TLB_MAX_ENTRIES * TLB_ENTRY_SIZE)

#define FIELD_VALID 0
#define FIELD_VADDR  4
#define FIELD_PAGE_DEC 8
#define FIELD_OFF_DEC 12
#define FIELD_PAGE_BIN 16
#define FIELD_OFF_BIN 20
#define FIELD_LAST_USED 24

static unsigned char *tlb_base = NULL;

static unsigned long global_use_counter = 0;

// Decimal a binario
void dec_to_bin(unsigned long value, int bits, char *out_buffer)
{
    for (int i = bits - 1; i >= 0; --i) {
        unsigned long bit = (value >> i) & 1UL;
        out_buffer[bits - 1 - i] = bit ? '1' : '0';
    }
    out_buffer[bits] = '\0';
}

// Binario a decimal
unsigned long bin_to_dec(const char *bin_str)
{
    unsigned long resultado = 0;
    size_t len = strlen(bin_str);

    for (size_t i = 0; i < len; ++i) {
        char c = bin_str[len - 1 - i];
        if (c == '1') {
            unsigned long potencia = (1UL << i);
            resultado += potencia;
        }
    }
    return resultado;
}

// TLB
void tlb_init(void)
{
    tlb_base = (unsigned char *)malloc(TLB_SIZE_BYTES);
    if (!tlb_base) {
        fprintf(stderr, "Error: no se pudo reservar memoria para TLB\n");
        exit(EXIT_FAILURE);
    }

    unsigned char *entry = tlb_base;
    for (int i = 0; i < TLB_MAX_ENTRIES; ++i) {
        int *valid = (int *)(entry + FIELD_VALID);
        *valid = 0;
        entry += TLB_ENTRY_SIZE;
    }
}

// Politica LRU
void tlb_lookup_and_update(uint32_t vaddr,
                           uint32_t page,
                           uint32_t offset,
                           uint32_t page_bin,
                           uint32_t offset_bin,
                           int *hit,
                           void **replaced_address,
                           unsigned long use_seq)
{
    unsigned char *entry = tlb_base;
    unsigned char *empty_entry = NULL;
    unsigned char *lru_entry = NULL;
    unsigned long lru_value = ULONG_MAX;

    for (int i = 0; i < TLB_MAX_ENTRIES; ++i) {
        if (*((int *)(entry + FIELD_VALID))) {
            
            unsigned long current_last_used =
                *((unsigned long *)(entry + FIELD_LAST_USED));

            if (*(uint32_t *)(entry + FIELD_VADDR) == vaddr) {
                *hit = 1;
                *replaced_address = NULL;
                *((unsigned long *)(entry + FIELD_LAST_USED)) = use_seq;
                return;
            }

            if (current_last_used < lru_value) {
                lru_entry = entry;
                lru_value = current_last_used;
            }
        } else {
            if (empty_entry == NULL) {
                empty_entry = entry;
            }
        }
        entry += TLB_ENTRY_SIZE;
    }

    *hit = 0;

    if (empty_entry != NULL) {
        entry = empty_entry;
        *replaced_address = NULL;

    } else {
        entry = lru_entry;
        *replaced_address = (void *)entry;
    }

    *((int *)(entry + FIELD_VALID)) = 1;
    *((uint32_t *)(entry + FIELD_VADDR)) = vaddr;
    *((uint32_t *)(entry + FIELD_PAGE_DEC)) = page;
    *((uint32_t *)(entry + FIELD_OFF_DEC)) = offset;
    *((uint32_t *)(entry + FIELD_PAGE_BIN)) = page_bin;
    *((uint32_t *)(entry + FIELD_OFF_BIN)) = offset_bin;
    *((unsigned long *)(entry + FIELD_LAST_USED)) = use_seq;
}

// Main
int main(void)
{
    char input_line[64];
    char *endptr;
    void *replaced_address = NULL;

    struct timeval start_time, end_time;

    tlb_init();

    unsigned char *tlb_start = tlb_base;
    unsigned char *tlb_end = tlb_base + TLB_SIZE_BYTES - 1;

    while (1) {
        printf("Ingrese dirección virtual: ");
        fflush(stdout);

        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            break;
        }

        if (input_line[0] == 's' || input_line[0] == 'S') {
            printf("Good bye!\n");
            break;
        }

        if (gettimeofday(&start_time, NULL) != 0) {
            perror("gettimeofday");
            free(tlb_base);
            return EXIT_FAILURE;
        }

        endptr = NULL;
        unsigned long long addr_ull = strtoull(input_line, &endptr, 10);

        if (endptr == input_line) {
            printf("Page Fault\n");
            continue;
        }

        if (addr_ull > 0xFFFFFFFFULL) {
            printf("Page Fault\n");
            continue;
        }

        uint32_t vaddr = (uint32_t)addr_ull;
        uint32_t page = vaddr >> PAGE_OFFSET_BITS;
        uint32_t offset = vaddr & (PAGE_SIZE - 1U);
        uint32_t page_bin_value = page;
        uint32_t offset_bin_value = offset;

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

        //Tiempo
        if (gettimeofday(&end_time, NULL) != 0) {
            perror("gettimeofday");
            free(tlb_base);
            return EXIT_FAILURE;
        }

        double elapsed = (double)(end_time.tv_sec - start_time.tv_sec)
                         + (double)(end_time.tv_usec - start_time.tv_usec) / 1000000.0;

        char page_bin_str[PAGE_NUMBER_BITS + 1];
        char offset_bin_str[PAGE_OFFSET_BITS + 1];

        dec_to_bin(page_bin_value, PAGE_NUMBER_BITS, page_bin_str);
        dec_to_bin(offset_bin_value, PAGE_OFFSET_BITS, offset_bin_str);

        //Salidas
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

        // Reemplazo
        if (replaced_address == NULL) {
            printf("Politica de reemplazo: 0x0\n");
        } else {
            printf("Politica de reemplazo: %p\n", replaced_address);
        }

        printf("Tiempo: %.6f segundos\n", elapsed);
        printf("\n");

    }
    free(tlb_base);
    return 0;
}