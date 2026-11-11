#ifndef HOST_LIBC_ASSERT_H
#define HOST_LIBC_ASSERT_H
void host_assert_fail(const char *expr, const char *file, int line);
#define assert(expr) ((expr) ? (void)0 : host_assert_fail(#expr, __FILE__, __LINE__))
#endif

