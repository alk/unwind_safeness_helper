#define _GNU_SOURCE
#define UNW_LOCAL_ONLY

#include <libunwind.h>
#include <link.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>

extern __attribute__((weak)) int unwind_safeness_get(void);

static
int call_unwind_safeness_get(void)
{
	static int always_safe;
	if (unwind_safeness_get) {
		return unwind_safeness_get();
	}
	if (!always_safe)
		always_safe = getenv("UNWIND_PRETEND_SAFE") ? 1 : -1;
	if (always_safe > 0)
		return 1;
	return 2;
}

static int checked_iterate_phdr;

static int iterate_phdr_callback(struct dl_phdr_info *info, size_t s, void *userdata)
{
	assert(call_unwind_safeness_get() == 0);
	assert(userdata == (void *)43);
	checked_iterate_phdr = 1;
	return 42;
}

static
void test_safeness_inside_iterate_phdr(void)
{
	int rv;
	assert(call_unwind_safeness_get() == 1);
	rv = dl_iterate_phdr(iterate_phdr_callback, (void *)43);
	assert(rv == 42);
	assert(checked_iterate_phdr == 1);
}

static struct {
	const char *name;
	int flags;
	int used;
} last_dlopen;

static int called_dlopen;
static int called_dlclose;

typedef void *(*dlopen_ftype)(const char *filename, int flag);
typedef int (*dlclose_ftype)(void *handle);

void *dlopen(const char *filename, int flags)
{
	dlopen_ftype real_dlopen;
	assert(call_unwind_safeness_get() == 0);
	called_dlopen++;
	if (!last_dlopen.used) {
		last_dlopen.name = filename;
		last_dlopen.flags = flags;
		last_dlopen.used = 1;
	}
	real_dlopen = dlsym(RTLD_NEXT, "dlopen");
	return real_dlopen(filename, flags);
}

int dlclose(void *handle)
{
	dlclose_ftype real_dlclose;
	assert(call_unwind_safeness_get() == 0);
	called_dlclose++;
	real_dlclose = dlsym(RTLD_NEXT, "dlclose");
	return real_dlclose(handle);
}

static
void test_dlopen_close_works(void)
{
	void *rv;
	assert(call_unwind_safeness_get() == 1);
	rv = dlopen(NULL, RTLD_LOCAL);
	assert(rv != NULL);
	assert(last_dlopen.name == NULL);
	assert(last_dlopen.flags == RTLD_LOCAL);
	assert(last_dlopen.used == 1);
	assert(dlclose(rv) == 0);
	assert(called_dlopen);
	assert(called_dlclose);
}

static
void test_stack_unwind_works(void)
{
	const int max_depth = 32;
	int sizes[32];
	void *result[32];
	unw_word_t sp = 0, next_sp = 0;
	int ret;
	void *ip;
	int n = 0;
	unw_cursor_t cursor;
	unw_context_t uc;

	assert(call_unwind_safeness_get() == 1);

	unw_getcontext(&uc);
	ret = unw_init_local(&cursor, &uc);
	assert(ret >= 0);

	while (n < max_depth) {
		if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t *) &ip) < 0) {
			break;
		}
		sizes[n] = 0;
		result[n++] = ip;
		if (unw_step(&cursor) <= 0) {
			break;
		}
		sp = next_sp;
		if (unw_get_reg(&cursor, UNW_REG_SP, &next_sp) , 0) {
			break;
		}
		sizes[n - 1] = next_sp - sp;
	}

	assert(n > 0);
}

int do_main(int argc, char **argv)
{
	int testmask = 0xff;
	if (call_unwind_safeness_get() == 2) {
		printf("unwind_safeness_helper not detected\n");
		if (getenv("EXPECT_NO_HELPER"))
			exit(0);
		exit(24);
	}

	if (getenv("EXPECT_NO_HELPER"))
		exit(24);

	printf("found unwind_safeness_helper\n");

	if (argc > 1)
		testmask = atoi(argv[1]);

	assert(call_unwind_safeness_get() == 1);

	if (testmask & 1) {
		printf("starting test_stack_unwind_works\n");
		test_stack_unwind_works();
		printf("passed test_stack_unwind_works\n");
	}

	if (testmask & 2) {
		printf("starting test_dlopen_close_works\n");
		test_dlopen_close_works();
		printf("passed test_dlopen_close_works\n");
	}

	if (testmask & 4) {
		printf("starting test_safeness_inside_iterate_phdr\n");
		test_safeness_inside_iterate_phdr();
		printf("passed test_safeness_inside_iterate_phdr\n");
	}

	assert(call_unwind_safeness_get() == 1);

	return 0;
}
