#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

unsigned long bin_to_dec(const char *bin_str)
{
    unsigned long result = 0;
    size_t len = strlen(bin_str);

    for (size_t i = 0; i < len; ++i) {
        char c = bin_str[len - 1 - i]; /* posición i desde la derecha */
        if (c == '1') {
            /* 2^i */
            unsigned long power_of_two = (1UL << i);
            result += power_of_two;
        }
    }
    return result;
}


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

    while (1) {{
                char input_line[64];
                printf("Ingrese dirección virtual: ");
                fflush(stdout);               // fuerza mostrar prompt
                if (fgets(input_line, sizeof(input_line), stdin) != NULL) {
                    char *endptr = NULL;
                    unsigned long long addr = strtoull(input_line, &endptr, 10);
                    if (endptr != input_line) {
                        // addr válido
                        printf("Página en binario: %s\n", bin_to_dec(page_bin_value, PAGE_NUMBER_BITS, page_bin_str));
                    } else {
                        // entrada inválida

                    }
                }
    }}


}