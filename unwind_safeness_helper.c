#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <link.h>
#include <dlfcn.h>
#include <libunwind.h>

#define EXPORT_API __attribute__((visibility("default")))

typedef int (*dl_iterate_phdr_cbtype)(struct dl_phdr_info *, size_t, void *);

typedef void *(*dlopen_ftype)(const char *filename, int flag);
typedef int (*dlclose_ftype)(void *handle);

typedef int (*dl_iterate_phdr_ftype)(dl_iterate_phdr_cbtype callback,
				     void *data);

typedef int (*unw_init_local_ftype)(unw_cursor_t *, unw_context_t *);
typedef int (*unw_get_reg_ftype)(unw_cursor_t *, unw_regnum_t, unw_word_t *);
typedef int (*unw_step_ftype)(unw_cursor_t *);

static dlopen_ftype orig_dlopen;
static dlclose_ftype orig_dlclose;
static dl_iterate_phdr_ftype orig_dl_iterate_phdr;
static unw_init_local_ftype orig_unw_init_local;
static unw_get_reg_ftype orig_unw_get_reg;
static unw_step_ftype orig_unw_step;

static __thread int unsafeness_depth;

static
void *must_dlsym(const char *sym)
{
	void *rv;
	char *error;
	dlerror();
	rv = dlsym(RTLD_NEXT, sym);
	error = dlerror();
	if (error) {
		fprintf(stderr, "failed to find symbol %s: %s\n", sym, error);
		abort();
	}
	return rv;
}


static __attribute__((constructor))
void init(void)
{
	orig_dlopen = must_dlsym("dlopen");
	orig_dlclose = must_dlsym("dlclose");
	orig_dl_iterate_phdr = must_dlsym("dl_iterate_phdr");
	orig_unw_init_local = must_dlsym("unw_init_local");
	orig_unw_get_reg = must_dlsym("unw_get_reg");
	orig_unw_step = must_dlsym("unw_step");
}

#define wrap_call(func, ...)				\
	({						\
		__typeof__((func)(__VA_ARGS__)) __rv;	\
		unsafeness_depth++;			\
		__rv = (func)(__VA_ARGS__);		\
		unsafeness_depth--;			\
		__rv;					\
	})

EXPORT_API
void *dlopen(const char *filename, int flag)
{
	return wrap_call(orig_dlopen, filename, flag);
}

EXPORT_API
int dlclose(void *handle)
{
	return wrap_call(orig_dlclose, handle);
}

EXPORT_API
int dl_iterate_phdr(dl_iterate_phdr_cbtype callback, void *data)
{
	return wrap_call(orig_dl_iterate_phdr, callback, data);
}

EXPORT_API
int unw_init_local(unw_cursor_t *cursor, unw_context_t *context)
{
	return wrap_call(orig_unw_init_local, cursor, context);
}

EXPORT_API
int unw_step(unw_cursor_t *cursor)
{
	if (unsafeness_depth)
		return 0;
	return wrap_call(unw_step, cursor);
}

EXPORT_API
int unw_get_reg(unw_cursor_t *cursor, unw_regnum_t reg, unw_word_t *out)
{
	if (unsafeness_depth)
		return -UNW_EUNSPEC;
	return wrap_call(unw_get_reg, cursor, reg, out);
}

EXPORT_API
int unwind_safeness_get(void)
{
	return !unsafeness_depth;
}
