// https://github.com/git/git/blob/9585ed519c59d5ac46f8e12b339220cdd0567b7d/compat/mingw.h
#include <dirent.h>
#include <stdio.h>

/*
 * A replacement of readdir, to ensure that it reads the file type at
 * the same time. This avoid extra unneeded lstats in git on MinGW
 */
#undef DT_UNKNOWN
#undef DT_DIR
#undef DT_REG
#undef DT_LNK
#define DT_UNKNOWN	0
#define DT_DIR		1
#define DT_REG		2
#define DT_LNK		3

struct mingw_dirent
{
	long		d_ino;			/* Always zero. */
	union {
		unsigned short	d_reclen;	/* Always zero. */
		unsigned char   d_type;		/* Reimplementation adds this */
	};
	unsigned short	d_namlen;		/* Length of name in d_name. */
	char		d_name[FILENAME_MAX];	/* File name. */
};
#define dirent mingw_dirent
#define readdir(x) mingw_readdir(x)
struct dirent *mingw_readdir(DIR *dir);
