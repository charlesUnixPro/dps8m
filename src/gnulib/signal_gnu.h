/* A GNU-like <signal.h>.

   Copyright (C) 2006-2016 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */
 
#define _POSIX
#include <signal.h>
#include <sys/types.h>

#ifndef _UID_T_
#define	_UID_T_
#ifndef _WIN64
typedef int	_uid_t;
#else
__MINGW_EXTENSION
typedef __int64	_uid_t;
#endif

#ifndef	NO_OLDNAMES
#undef uid_t
typedef _uid_t	uid_t;
#endif
#endif	/* Not _UID_T_ */

/* Present to allow compilation, but unsupported by gnulib.  */
union sigval
{
  int sival_int;
  void *sival_ptr;
};
 
/* Present to allow compilation, but unsupported by gnulib.  */
struct siginfo_t
{
  int si_signo;
  int si_code;
  int si_errno;
  pid_t si_pid;
  uid_t si_uid;
  void *si_addr;
  int si_status;
  long si_band;
  union sigval si_value;
};
typedef struct siginfo_t siginfo_t; 

struct sigaction
{
  union
  {
    void (*_sa_handler) (int);
    /* Present to allow compilation, but unsupported by gnulib.  POSIX
       says that implementations may, but not must, make sa_sigaction
       overlap with sa_handler, but we know of no implementation where
       they do not overlap.  */
    void (*_sa_sigaction) (int, siginfo_t *, void *);
  } _sa_func;
  sigset_t sa_mask;
  /* Not all POSIX flags are supported.  */
  int sa_flags;
};
#   define sa_handler _sa_func._sa_handler
#   define sa_sigaction _sa_func._sa_sigaction
/* Unsupported flags are not present.  */
#   define SA_RESETHAND 1
#   define SA_NODEFER 2
#   define SA_RESTART 4

#  define SIG_BLOCK   0  /* blocked_set = blocked_set | *set; */
#  define SIG_SETMASK 1  /* blocked_set = *set; */
#  define SIG_UNBLOCK 2  /* blocked_set = blocked_set & ~*set; */

int
sigaction (int sig, const struct sigaction */*restrict*/ act,
           struct sigaction */*restrict*/ oact);

#define SIGUSR1 30 /* user defined signal 1 */
