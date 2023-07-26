// TODO: header

# include "omp_task.h"

#if 0
# define _GNU_SOURCE
# include <link.h>

/**
 * Check dynamic symbols loaded on the fly. This callback is called once per
 * shared library loaded
 * https://stackoverflow.com/questions/15779185/how-to-list-on-the-fly-all-the-functions-symbols-available-in-c-code-on-a-linux
 *
 * man dl_iterate_phdr
 */
static int
omp_task_load_dynamic_symbols(
    struct dl_phdr_info * info,
    size_t size,
    void * data
) {
    OMP_DEBUG("Name: \"%s\" (%d segments)\n", info->dlpi_name, info->dlpi_phnum);

    /* Iterate over all headers of the current shared lib */
    for (ElfW(Half) header_index = 0; header_index < info->dlpi_phnum; header_index++)
    {
        switch (info->dlpi_phdr[header_index].p_type)
        {
            case (PT_DYNAMIC):
            {
                /* Get a pointer to the first entry of the dynamic section.
                 * It's address is the shared lib's address + the virtual address */
                ElfW(Dyn) * dyn = (ElfW(Dyn) *)(info->dlpi_addr + info->dlpi_phdr[header_index].p_vaddr);

                char * strtab       = NULL;
                ElfW(Sym) * symtab  = NULL;
                ElfW(Word) * hash   = NULL;

                while (dyn->d_tag != DT_NULL)
                {
                    switch (dyn->d_tag)
                    {
                        case (DT_HASH):
                        {
                            hash = (ElfW(Word) *) dyn->d_un.d_ptr;
                            break ;
                        }
                        case (DT_STRTAB):
                        {
                            strtab = (char*) dyn->d_un.d_ptr;
                            break ;
                        }
                        case (DT_SYMTAB):
                        {
                            symtab = (ElfW(Sym) *) dyn->d_un.d_ptr;
                            break ;
                        }

                        case (DT_NULL):
                        default:
                        {
                            break ;
                        }
                    }
                    ++dyn;
                }

                if (hash && strtab && symtab)
                {
                    /* Iterate over the symbol table */
                    ElfW(Word) sym_cnt = hash[1];
                    for (ElfW(Word) sym_index = 0; sym_index < sym_cnt; sym_index++)
                    {
                        /* get the name of the i-th symbol.
                         * This is located at the address of st_name
                         * relative to the beginning of the string table. */
                        char * sym_name = &strtab[symtab[sym_index].st_name];
                        OMP_DEBUG("%s\n", sym_name);
                    }
                }
                break ;
            }

            default:
            {
                break ;
            }
        }
    }
    return 0;
}

void
omp_task_load_symbols(void)
{
    dl_iterate_phdr(omp_task_load_dynamic_symbols, NULL);
}

#else

void
omp_task_load_symbols(void)
{
}

#endif
