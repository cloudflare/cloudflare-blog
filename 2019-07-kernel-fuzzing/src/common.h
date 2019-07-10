#define PFATAL(x...)                                                           \
	do {                                                                   \
		fprintf(stderr, "[-] SYSTEM ERROR : " x);                      \
		fprintf(stderr, "\n\tLocation : %s(), %s:%u\n", __FUNCTION__,  \
			__FILE__, __LINE__);                                   \
		perror("      OS message ");                                   \
		fprintf(stderr, "\n");                                         \
		exit(EXIT_FAILURE);                                            \
	} while (0)

/* siphash.c */
uint32_t hsiphash_static(const void *src, unsigned long src_sz);

/* kcov.c */
struct kcov;
struct kcov *kcov_new(void);
void kcov_enable(struct kcov *kcov);
int kcov_disable(struct kcov *kcov);
void kcov_free(struct kcov *kcov);
uint64_t *kcov_cover(struct kcov *kcov);

/* forksrv.c */
struct forksrv;
struct forksrv *forksrv_new(void);
int forksrv_on(struct forksrv *forksrv);
void forksrv_welcome(struct forksrv *forksrv);
int32_t forksrv_cycle(struct forksrv *forksrv, uint32_t child_pid);
void forksrv_status(struct forksrv *forksrv, uint32_t status);
uint8_t *forksrv_area_ptr(struct forksrv *forksrv);
void forksrv_free(struct forksrv *forksrv);

/* utils.c */
struct option;
const char *optstring_from_long_options(const struct option *opt);
int taskset(int taskset_cpu);

/* namespace.c */
int netns_save();
void netns_new();
void netns_restore(int net_ns);
