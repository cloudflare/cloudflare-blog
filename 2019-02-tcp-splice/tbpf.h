#define KERNEL_VERSION(a, b, c) ((a)*65536 + (b)*256 + (c))

/* tbpf.c */

/* See https://lkml.org/lkml/2014/8/13/116 and
 * https://patchwork.ozlabs.org/patch/930413/ for the reocation type
 * BPF_PSEUDO_MAP_FD or R_BPF_MAP_FD with value 1 */

/* Relocations, as exposed in format consumeable by C */
struct tbpf_reloc {
	char *name; /* Name of the symbol */
	int type;   /* Type of relocation, expected 1 */
	int offset; /* Offset: ebpf instruction number */
};

int tbpf_fill_symbol(struct bpf_insn *insns, struct tbpf_reloc *relocs,
		     const char *symbol, int32_t value);

int tbpf_create_map(enum bpf_map_type map_type, int key_size, int value_size,
		    int max_entries, uint32_t map_flags);
int tbpf_load_program(enum bpf_prog_type prog_type,
		      const struct bpf_insn *insns, size_t insns_cnt,
		      const char *license, uint32_t kern_version, char *log_buf,
		      size_t log_buf_sz);
int tbpf_prog_attach(int prog_fd, int target_fd, enum bpf_attach_type type,
		     unsigned int flags);
int tbpf_map_update_elem(int fd, const void *key, const void *value,
			 uint64_t flags);
int tbpf_map_delete_elem(int fd, const void *key);
int tbpf_map_lookup_elem(int fd, const void *key, void *value);
