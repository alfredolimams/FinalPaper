
/* auto-generated by gen_syscalls.py, don't edit */
#ifndef Z_INCLUDE_SYSCALLS_SOCKET_H
#define Z_INCLUDE_SYSCALLS_SOCKET_H


#ifndef _ASMLANGUAGE

#include <syscall_list.h>
#include <syscall_macros.h>

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int z_impl_zsock_socket(int family, int type, int proto);
static inline int zsock_socket(int family, int type, int proto)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke3(*(uintptr_t *)&family, *(uintptr_t *)&type, *(uintptr_t *)&proto, K_SYSCALL_ZSOCK_SOCKET);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_socket(family, type, proto);
}


extern int z_impl_zsock_close(int sock);
static inline int zsock_close(int sock)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke1(*(uintptr_t *)&sock, K_SYSCALL_ZSOCK_CLOSE);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_close(sock);
}


extern int z_impl_zsock_shutdown(int sock, int how);
static inline int zsock_shutdown(int sock, int how)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke2(*(uintptr_t *)&sock, *(uintptr_t *)&how, K_SYSCALL_ZSOCK_SHUTDOWN);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_shutdown(sock, how);
}


extern int z_impl_zsock_bind(int sock, const struct sockaddr * addr, socklen_t addrlen);
static inline int zsock_bind(int sock, const struct sockaddr * addr, socklen_t addrlen)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke3(*(uintptr_t *)&sock, *(uintptr_t *)&addr, *(uintptr_t *)&addrlen, K_SYSCALL_ZSOCK_BIND);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_bind(sock, addr, addrlen);
}


extern int z_impl_zsock_connect(int sock, const struct sockaddr * addr, socklen_t addrlen);
static inline int zsock_connect(int sock, const struct sockaddr * addr, socklen_t addrlen)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke3(*(uintptr_t *)&sock, *(uintptr_t *)&addr, *(uintptr_t *)&addrlen, K_SYSCALL_ZSOCK_CONNECT);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_connect(sock, addr, addrlen);
}


extern int z_impl_zsock_listen(int sock, int backlog);
static inline int zsock_listen(int sock, int backlog)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke2(*(uintptr_t *)&sock, *(uintptr_t *)&backlog, K_SYSCALL_ZSOCK_LISTEN);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_listen(sock, backlog);
}


extern int z_impl_zsock_accept(int sock, struct sockaddr * addr, socklen_t * addrlen);
static inline int zsock_accept(int sock, struct sockaddr * addr, socklen_t * addrlen)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke3(*(uintptr_t *)&sock, *(uintptr_t *)&addr, *(uintptr_t *)&addrlen, K_SYSCALL_ZSOCK_ACCEPT);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_accept(sock, addr, addrlen);
}


extern ssize_t z_impl_zsock_sendto(int sock, const void * buf, size_t len, int flags, const struct sockaddr * dest_addr, socklen_t addrlen);
static inline ssize_t zsock_sendto(int sock, const void * buf, size_t len, int flags, const struct sockaddr * dest_addr, socklen_t addrlen)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (ssize_t) arch_syscall_invoke6(*(uintptr_t *)&sock, *(uintptr_t *)&buf, *(uintptr_t *)&len, *(uintptr_t *)&flags, *(uintptr_t *)&dest_addr, *(uintptr_t *)&addrlen, K_SYSCALL_ZSOCK_SENDTO);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_sendto(sock, buf, len, flags, dest_addr, addrlen);
}


extern ssize_t z_impl_zsock_sendmsg(int sock, const struct msghdr * msg, int flags);
static inline ssize_t zsock_sendmsg(int sock, const struct msghdr * msg, int flags)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (ssize_t) arch_syscall_invoke3(*(uintptr_t *)&sock, *(uintptr_t *)&msg, *(uintptr_t *)&flags, K_SYSCALL_ZSOCK_SENDMSG);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_sendmsg(sock, msg, flags);
}


extern ssize_t z_impl_zsock_recvfrom(int sock, void * buf, size_t max_len, int flags, struct sockaddr * src_addr, socklen_t * addrlen);
static inline ssize_t zsock_recvfrom(int sock, void * buf, size_t max_len, int flags, struct sockaddr * src_addr, socklen_t * addrlen)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (ssize_t) arch_syscall_invoke6(*(uintptr_t *)&sock, *(uintptr_t *)&buf, *(uintptr_t *)&max_len, *(uintptr_t *)&flags, *(uintptr_t *)&src_addr, *(uintptr_t *)&addrlen, K_SYSCALL_ZSOCK_RECVFROM);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_recvfrom(sock, buf, max_len, flags, src_addr, addrlen);
}


extern int z_impl_zsock_fcntl(int sock, int cmd, int flags);
static inline int zsock_fcntl(int sock, int cmd, int flags)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke3(*(uintptr_t *)&sock, *(uintptr_t *)&cmd, *(uintptr_t *)&flags, K_SYSCALL_ZSOCK_FCNTL);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_fcntl(sock, cmd, flags);
}


extern int z_impl_zsock_poll(struct zsock_pollfd * fds, int nfds, int timeout);
static inline int zsock_poll(struct zsock_pollfd * fds, int nfds, int timeout)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke3(*(uintptr_t *)&fds, *(uintptr_t *)&nfds, *(uintptr_t *)&timeout, K_SYSCALL_ZSOCK_POLL);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_poll(fds, nfds, timeout);
}


extern int z_impl_zsock_getsockopt(int sock, int level, int optname, void * optval, socklen_t * optlen);
static inline int zsock_getsockopt(int sock, int level, int optname, void * optval, socklen_t * optlen)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke5(*(uintptr_t *)&sock, *(uintptr_t *)&level, *(uintptr_t *)&optname, *(uintptr_t *)&optval, *(uintptr_t *)&optlen, K_SYSCALL_ZSOCK_GETSOCKOPT);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_getsockopt(sock, level, optname, optval, optlen);
}


extern int z_impl_zsock_setsockopt(int sock, int level, int optname, const void * optval, socklen_t optlen);
static inline int zsock_setsockopt(int sock, int level, int optname, const void * optval, socklen_t optlen)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke5(*(uintptr_t *)&sock, *(uintptr_t *)&level, *(uintptr_t *)&optname, *(uintptr_t *)&optval, *(uintptr_t *)&optlen, K_SYSCALL_ZSOCK_SETSOCKOPT);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_setsockopt(sock, level, optname, optval, optlen);
}


extern int z_impl_zsock_getsockname(int sock, struct sockaddr * addr, socklen_t * addrlen);
static inline int zsock_getsockname(int sock, struct sockaddr * addr, socklen_t * addrlen)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke3(*(uintptr_t *)&sock, *(uintptr_t *)&addr, *(uintptr_t *)&addrlen, K_SYSCALL_ZSOCK_GETSOCKNAME);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_getsockname(sock, addr, addrlen);
}


extern int z_impl_zsock_gethostname(char * buf, size_t len);
static inline int zsock_gethostname(char * buf, size_t len)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke2(*(uintptr_t *)&buf, *(uintptr_t *)&len, K_SYSCALL_ZSOCK_GETHOSTNAME);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_gethostname(buf, len);
}


extern int z_impl_zsock_inet_pton(sa_family_t family, const char * src, void * dst);
static inline int zsock_inet_pton(sa_family_t family, const char * src, void * dst)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke3(*(uintptr_t *)&family, *(uintptr_t *)&src, *(uintptr_t *)&dst, K_SYSCALL_ZSOCK_INET_PTON);
	}
#endif
	compiler_barrier();
	return z_impl_zsock_inet_pton(family, src, dst);
}


extern int z_impl_z_zsock_getaddrinfo_internal(const char * host, const char * service, const struct zsock_addrinfo * hints, struct zsock_addrinfo * res);
static inline int z_zsock_getaddrinfo_internal(const char * host, const char * service, const struct zsock_addrinfo * hints, struct zsock_addrinfo * res)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		return (int) arch_syscall_invoke4(*(uintptr_t *)&host, *(uintptr_t *)&service, *(uintptr_t *)&hints, *(uintptr_t *)&res, K_SYSCALL_Z_ZSOCK_GETADDRINFO_INTERNAL);
	}
#endif
	compiler_barrier();
	return z_impl_z_zsock_getaddrinfo_internal(host, service, hints, res);
}


#ifdef __cplusplus
}
#endif

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#endif

#endif
#endif /* include guard */