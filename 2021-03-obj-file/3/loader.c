/* compile and link:
 * $ gcc -o loader loader.c
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* for open(2), fstat(2) */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* for close(2), fstat(2) */
#include <unistd.h>

/* for mmap(2) */
#include <sys/mman.h>

/* parsing ELF files */
#include <elf.h>

/* for errno */
#include <errno.h>

/* from https://elixir.bootlin.com/linux/v5.11.6/source/arch/x86/include/asm/elf.h#L51 */
#define R_X86_64_PC32 2
#define R_X86_64_PLT32 4

typedef union {
    const Elf64_Ehdr *hdr;
    const uint8_t *base;
} objhdr;

/* obj.o memory address */
static objhdr obj;

/* sections table */
static const Elf64_Shdr *sections;
static const char *shstrtab = NULL;

/* symbols table */
static const Elf64_Sym *symbols;
/* number of entries in the symbols table */
static int num_symbols;
static const char *strtab = NULL;

static uint64_t page_size;

/* runtime base address of the imported code */
static uint8_t *text_runtime_base;
/* runtime base of the .data section */
static uint8_t *data_runtime_base;
/* runtime base of the .rodata section */
static uint8_t *rodata_runtime_base;

/* number of external symbols in the symbol table */
static int num_ext_symbols = 0;

struct ext_jump {
    /* address to jump to */
    uint8_t *addr;
    /* unconditional x64 JMP instruction */
    /* should always be {0xff, 0x25, 0xf2, 0xff, 0xff, 0xff} */
    /* so it would jump to an address stored at addr above */
    uint8_t instr[6];
};

struct ext_jump *jumptable;

/* external dependencies for obj.o */
static int my_puts(const char *s)
{
    puts("my_puts executed");
    return puts(s);
}

static inline uint64_t page_align(uint64_t n)
{
    return (n + (page_size - 1)) & ~(page_size - 1);
}

static void load_obj(void)
{
    struct stat sb;

    int fd = open("obj.o", O_RDONLY);
    if (fd <= 0) {
        perror("Cannot open obj.o");
        exit(errno);
    }

    /* we need obj.o size for mmap(2) */
    if (fstat(fd, &sb)) {
        perror("Failed to get obj.o info");
        exit(errno);
    }

    /* mmap obj.o into memory */
    obj.base = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (obj.base == MAP_FAILED) {
        perror("Maping obj.o failed");
        exit(errno);
    }
    close(fd);
}

static const Elf64_Shdr *lookup_section(const char *name)
{
    size_t name_len = strlen(name);

    /* number of entries in the sections table is encoded in the ELF header */
    for (Elf64_Half i = 0; i < obj.hdr->e_shnum; i++) {
        /* sections table entry does not contain the string name of the section
         * instead, the `sh_name` parameter is an offset in the `.shstrtab`
         * section, which points to a string name
         */
        const char *section_name = shstrtab + sections[i].sh_name;
        size_t section_name_len = strlen(section_name);

        if (name_len == section_name_len && !strcmp(name, section_name)) {
            /* we ignore sections with 0 size */
            if (sections[i].sh_size)
                return sections + i;
        }
    }

    return NULL;
}

static void *lookup_function(const char *name)
{
    size_t name_len = strlen(name);

    /* loop through all the symbols in the symbol table */
    for (int i = 0; i < num_symbols; i++) {
        /* consider only function symbols */
        if (ELF64_ST_TYPE(symbols[i].st_info) == STT_FUNC) {
            /* symbol table entry does not contain the string name of the symbol
             * instead, the `st_name` parameter is an offset in the `.strtab`
             * section, which points to a string name
             */
            const char *function_name = strtab + symbols[i].st_name;
            size_t function_name_len = strlen(function_name);

            if (name_len == function_name_len && !strcmp(name, function_name)) {
                /* st_value is an offset in bytes of the function from the
                 * beginning of the `.text` section
                 */
                return text_runtime_base + symbols[i].st_value;
            }
        }
    }

    return NULL;
}

static void *lookup_ext_function(const char *name)
{
    size_t name_len = strlen(name);

    if (name_len == strlen("puts") && !strcmp(name, "puts"))
        return my_puts;

    fprintf(stderr, "No address for function %s\n", name);
    exit(ENOENT);
}

static void count_external_symbols(void)
{
    const Elf64_Shdr *rela_text_hdr = lookup_section(".rela.text");
    if (!rela_text_hdr) {
        fputs("Failed to find .rela.text\n", stderr);
        exit(ENOEXEC);
    }

    int num_relocations = rela_text_hdr->sh_size / rela_text_hdr->sh_entsize;
    const Elf64_Rela *relocations = (Elf64_Rela *)(obj.base + rela_text_hdr->sh_offset);

    for (int i = 0; i < num_relocations; i++) {
        int symbol_idx = ELF64_R_SYM(relocations[i].r_info);

        /* if there is no section associated with a symbol, it is probably
         * an external reference */
        if (symbols[symbol_idx].st_shndx == SHN_UNDEF)
            num_ext_symbols++;
    }
}

static uint8_t *section_runtime_base(const Elf64_Shdr *section)
{
    const char *section_name = shstrtab + section->sh_name;
    size_t section_name_len = strlen(section_name);

    if (strlen(".text") == section_name_len && !strcmp(".text", section_name))
        return text_runtime_base;

    if (strlen(".data") == section_name_len && !strcmp(".data", section_name))
        return data_runtime_base;

    if (strlen(".rodata") == section_name_len && !strcmp(".rodata", section_name))
        return rodata_runtime_base;

    fprintf(stderr, "No runtime base address for section %s\n", section_name);
    exit(ENOENT);
}

static void do_text_relocations(void)
{
    /* we actually cheat here - the name .rela.text is a convention, but not a
     * rule: to figure out which section should be patched by these relocations
     * we would need to examine the rela_text_hdr, but we skip it for simplicity
     */
    const Elf64_Shdr *rela_text_hdr = lookup_section(".rela.text");
    if (!rela_text_hdr) {
        fputs("Failed to find .rela.text\n", stderr);
        exit(ENOEXEC);
    }

    int num_relocations = rela_text_hdr->sh_size / rela_text_hdr->sh_entsize;
    const Elf64_Rela *relocations = (Elf64_Rela *)(obj.base + rela_text_hdr->sh_offset);

    for (int i = 0; i < num_relocations; i++) {
        int symbol_idx = ELF64_R_SYM(relocations[i].r_info);
        int type = ELF64_R_TYPE(relocations[i].r_info);

        /* where to patch .text */
        uint8_t *patch_offset = text_runtime_base + relocations[i].r_offset;
        /* symbol, with respect to which the relocation is performed */
        uint8_t *symbol_address;
        
        /* if this is an external symbol */
        if (symbols[symbol_idx].st_shndx == SHN_UNDEF) {
            static int curr_jmp_idx = 0;

            /* get external symbol/function address by name */
            jumptable[curr_jmp_idx].addr = lookup_ext_function(strtab +  symbols[symbol_idx].st_name);

            /* x64 unconditional JMP with address stored at -14 bytes offset */
            /* will use the address stored in addr above */
            jumptable[curr_jmp_idx].instr[0] = 0xff;
            jumptable[curr_jmp_idx].instr[1] = 0x25;
            jumptable[curr_jmp_idx].instr[2] = 0xf2;
            jumptable[curr_jmp_idx].instr[3] = 0xff;
            jumptable[curr_jmp_idx].instr[4] = 0xff;
            jumptable[curr_jmp_idx].instr[5] = 0xff;

            /* resolve the relocation with respect to this unconditional JMP */
            symbol_address = (uint8_t *)(&jumptable[curr_jmp_idx].instr);

            curr_jmp_idx++;
        } else {
            symbol_address = section_runtime_base(&sections[symbols[symbol_idx].st_shndx]) + symbols[symbol_idx].st_value;
        }

        switch (type)
        {
        case R_X86_64_PC32:
            /* S + A - P, 32 bit output, S == L here */
        case R_X86_64_PLT32:
            /* L + A - P, 32 bit output */
            *((uint32_t *)patch_offset) = symbol_address + relocations[i].r_addend - patch_offset;
            break;
        }
    }
}

static void parse_obj(void)
{
    /* the sections table offset is encoded in the ELF header */
    sections = (const Elf64_Shdr *)(obj.base + obj.hdr->e_shoff);
    /* the index of `.shstrtab` in the sections table is encoded in the ELF header
     * so we can find it without actually using a name lookup
     */
    shstrtab = (const char *)(obj.base + sections[obj.hdr->e_shstrndx].sh_offset);

    /* find the `.symtab` entry in the sections table */
    const Elf64_Shdr *symtab_hdr = lookup_section(".symtab");
    if (!symtab_hdr) {
        fputs("Failed to find .symtab\n", stderr);
        exit(ENOEXEC);
    }

    /* the symbols table */
    symbols = (const Elf64_Sym *)(obj.base + symtab_hdr->sh_offset);
    /* number of entries in the symbols table = table size / entry size */
    num_symbols = symtab_hdr->sh_size / symtab_hdr->sh_entsize;

    const Elf64_Shdr *strtab_hdr = lookup_section(".strtab");
    if (!strtab_hdr) {
        fputs("Failed to find .strtab\n", stderr);
        exit(ENOEXEC);
    }

    strtab = (const char *)(obj.base + strtab_hdr->sh_offset);

    /* get system page size */
    page_size = sysconf(_SC_PAGESIZE);

    /* find the `.text` entry in the sections table */
    const Elf64_Shdr *text_hdr = lookup_section(".text");
    if (!text_hdr) {
        fputs("Failed to find .text\n", stderr);
        exit(ENOEXEC);
    }

    /* find the `.data` entry in the sections table */
    const Elf64_Shdr *data_hdr = lookup_section(".data");
    if (!data_hdr) {
        fputs("Failed to find .data\n", stderr);
        exit(ENOEXEC);
    }

    /* find the `.rodata` entry in the sections table */
    const Elf64_Shdr *rodata_hdr = lookup_section(".rodata");
    if (!rodata_hdr) {
        fputs("Failed to find .rodata\n", stderr);
        exit(ENOEXEC);
    }

    count_external_symbols();

    /* allocate memory for `.text`, `.data` and `.rodata` copies and the "jumptable" for external symbols, rounding up each section to whole pages */
    text_runtime_base = mmap(NULL, page_align(text_hdr->sh_size) + \
                                   page_align(data_hdr->sh_size) + \
                                   page_align(rodata_hdr->sh_size) + \
                                   page_align(sizeof(struct ext_jump) * num_ext_symbols),
                                   PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (text_runtime_base == MAP_FAILED) {
        perror("Failed to allocate memory");
        exit(errno);
    }

    /* .data will come right after .text */
    data_runtime_base = text_runtime_base + page_align(text_hdr->sh_size);
    /* .rodata will come after .data */
    rodata_runtime_base = data_runtime_base + page_align(data_hdr->sh_size);
    /* jumptable will come after .rodata */
    jumptable = (struct ext_jump *)(rodata_runtime_base + page_align(rodata_hdr->sh_size));

    /* copy the contents of `.text` section from the ELF file */
    memcpy(text_runtime_base, obj.base + text_hdr->sh_offset, text_hdr->sh_size);
    /* copy .data */
    memcpy(data_runtime_base, obj.base + data_hdr->sh_offset, data_hdr->sh_size);
    /* copy .rodata */
    memcpy(rodata_runtime_base, obj.base + rodata_hdr->sh_offset, rodata_hdr->sh_size);

    do_text_relocations();

    /* make the `.text` copy readonly and executable */
    if (mprotect(text_runtime_base, page_align(text_hdr->sh_size), PROT_READ | PROT_EXEC)) {
        perror("Failed to make .text executable");
        exit(errno);
    }

    /* we don't need to do anything with .data - it should remain read/write */

    /* make the `.rodata` copy readonly */
    if (mprotect(rodata_runtime_base, page_align(rodata_hdr->sh_size), PROT_READ)) {
        perror("Failed to make .rodata readonly");
        exit(errno);
    }

    /* make the jumptable readonly and executable */
    if (mprotect(jumptable, page_align(sizeof(struct ext_jump) * num_ext_symbols), PROT_READ | PROT_EXEC)) {
        perror("Failed to make the jumptable executable");
        exit(errno);
    }
}

static void execute_funcs(void)
{
    /* pointers to imported functions */
    int (*add5)(int);
    int (*add10)(int);
    const char *(*get_hello)(void);
    int (*get_var)(void);
    void (*set_var)(int num);
    void (*say_hello)(void);

    add5 = lookup_function("add5");
    if (!add5) {
        fputs("Failed to find add5 function\n", stderr);
        exit(ENOENT);
    }

    puts("Executing add5...");
    printf("add5(%d) = %d\n", 42, add5(42));

    add10 = lookup_function("add10");
    if (!add10) {
        fputs("Failed to find add10 function\n", stderr);
        exit(ENOENT);
    }

    puts("Executing add10...");
    printf("add10(%d) = %d\n", 42, add10(42));

    get_hello = lookup_function("get_hello");
    if (!get_hello) {
        fputs("Failed to find get_hello function\n", stderr);
        exit(ENOENT);
    }

    puts("Executing get_hello...");
    printf("get_hello() = %s\n", get_hello());

    get_var = lookup_function("get_var");
    if (!get_var) {
        fputs("Failed to find get_var function\n", stderr);
        exit(ENOENT);
    }

    puts("Executing get_var...");
    printf("get_var() = %d\n", get_var());

    set_var = lookup_function("set_var");
    if (!set_var) {
        fputs("Failed to find set_var function\n", stderr);
        exit(ENOENT);
    }

    puts("Executing set_var(42)...");
    set_var(42);

    puts("Executing get_var again...");
    printf("get_var() = %d\n", get_var());

    say_hello = lookup_function("say_hello");
    if (!say_hello) {
        fputs("Failed to find say_hello function\n", stderr);
        exit(ENOENT);
    }

    puts("Executing say_hello...");
    say_hello();
}

int main(void)
{
    load_obj();
    parse_obj();
    execute_funcs();

    return 0;
}
