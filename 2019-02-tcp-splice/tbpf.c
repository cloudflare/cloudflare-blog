/*
 * This code is based on bpf.c from:
 * https://github.com/torvalds/linux/blob/master/tools/lib/bpf/bpf.c
 *
 * but as opposed to bpf.c or libbpf.c it does not have a dependency
 * on libelf.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "tbpf.h"

static inline int sys_bpf(enum bpf_cmd cmd, union bpf_attr *attr,
			  unsigned int size)
{
	return syscall(__NR_bpf, cmd, attr, size);
}

/* Fixup a relocation in ebpf bpf_insn table. */
int tbpf_fill_symbol(struct bpf_insn *insns, struct tbpf_reloc *relocs,
		     const char *symbol, int32_t value)
{
	int c = 0;
	while (relocs && relocs->name && relocs->name[0] != '\x00') {
		if (strcmp(relocs->name, symbol) == 0) {
			switch (relocs->type) {
			case 1:
				insns[relocs->offset].src_reg = 1;
				insns[relocs->offset].imm = value;
				c += 1;
				break;
			default:
				fprintf(stderr,
					"FATAL: unknown relocation %d\n",
					relocs->type);
				abort();
			}
		}
		relocs++;
	}
	return c;
}

int tbpf_create_map(enum bpf_map_type map_type, int key_size, int value_size,
		    int max_entries, uint32_t map_flags)
{
	union bpf_attr attr = {};

	attr.map_type = map_type;
	attr.key_size = key_size;
	attr.value_size = value_size;
	attr.max_entries = max_entries;
	attr.map_flags = map_flags;
	return sys_bpf(BPF_MAP_CREATE, &attr, sizeof(attr));
}

int tbpf_load_program(enum bpf_prog_type prog_type,
		      const struct bpf_insn *insns, size_t insns_cnt,
		      const char *license, uint32_t kern_version, char *log_buf,
		      size_t log_buf_sz)
{
	union bpf_attr attr = {};

	attr.prog_type = prog_type;
	attr.insns = (uint64_t)insns;
	attr.insn_cnt = insns_cnt;
	attr.license = (uint64_t)license;
	attr.log_buf = (uint64_t)NULL;
	attr.log_size = 0;
	attr.log_level = 0;
	attr.kern_version = kern_version;

	int fd = sys_bpf(BPF_PROG_LOAD, &attr, sizeof(attr));
	if (fd >= 0 || !log_buf || !log_buf_sz)
		return fd;

	/* Try again with log */
	attr.log_buf = (uint64_t)log_buf;
	attr.log_size = log_buf_sz;
	attr.log_level = 1;
	log_buf[0] = 0;
	return sys_bpf(BPF_PROG_LOAD, &attr, sizeof(attr));
}

int tbpf_prog_attach(int prog_fd, int target_fd, enum bpf_attach_type type,
		     unsigned int flags)
{
	union bpf_attr attr = {};

	attr.target_fd = target_fd;
	attr.attach_bpf_fd = prog_fd;
	attr.attach_type = type;
	attr.attach_flags = flags;

	return sys_bpf(BPF_PROG_ATTACH, &attr, sizeof(attr));
}

int tbpf_map_update_elem(int fd, const void *key, const void *value,
			 uint64_t flags)
{
	union bpf_attr attr = {};

	attr.map_fd = fd;
	attr.key = (uint64_t)key;
	attr.value = (uint64_t)value;
	attr.flags = flags;

	return sys_bpf(BPF_MAP_UPDATE_ELEM, &attr, sizeof(attr));
}

int tbpf_map_delete_elem(int fd, const void *key)
{
	union bpf_attr attr = {};

	attr.map_fd = fd;
	attr.key = (uint64_t)key;

	return sys_bpf(BPF_MAP_DELETE_ELEM, &attr, sizeof(attr));
}

int tbpf_map_lookup_elem(int fd, const void *key, void *value)
{
	union bpf_attr attr = {};

	attr.map_fd = fd;
	attr.key = (uint64_t)key;
	attr.value = (uint64_t)value;

	return sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr, sizeof(attr));
}
