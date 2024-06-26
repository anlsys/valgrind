#ifndef __MALLOC_H__
# define __MALLOC_H__

void * taskgrind_malloc(ThreadId tid, SizeT n);
void * taskgrind___builtin_new(ThreadId tid, SizeT n);
void * taskgrind___builtin_new_aligned(ThreadId tid, SizeT n, SizeT align, SizeT orig_align);
void * taskgrind___builtin_vec_new(ThreadId tid, SizeT n);
void * taskgrind___builtin_vec_new_aligned(ThreadId tid, SizeT n, SizeT align, SizeT orig_align);
void * taskgrind_memalign(ThreadId tid, SizeT align, SizeT orig_align, SizeT n);
void * taskgrind_calloc(ThreadId tid, SizeT nmemb, SizeT size);
void taskgrind_free(ThreadId tid, void * p);
void taskgrind___builtin_delete(ThreadId tid, void * p);
void taskgrind___builtin_delete_aligned(ThreadId tid, void * p, SizeT align);
void taskgrind___builtin_vec_delete(ThreadId tid, void * p);
void taskgrind___builtin_vec_delete_aligned(ThreadId tid, void* p, SizeT align);
void * taskgrind_realloc(ThreadId tid, void* p_old, SizeT new_szB);
SizeT taskgrind_malloc_usable_size(ThreadId tid, void * p);

#endif
