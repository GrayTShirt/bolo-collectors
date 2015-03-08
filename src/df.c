#include <stdio.h>
#include <vigor.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROC "/proc"

static const char *PREFIX;

int collect_mounts(void);

int main(int argc, char **argv)
{
	if (argc > 2) {
		fprintf(stderr, "USAGE: %s prefix\n", argv[0]);
		exit(1);
	}

	PREFIX = (argc == 2) ? argv[1] : fqdn();
	return collect_mounts();
}

int collect_mounts(void)
{
	FILE *io = fopen(PROC "/mounts", "r");
	if (!io)
		return 1;

	struct stat st;
	struct statvfs fs;
	hash_t seen = {0};
	char *a, *b, *c, buf[8192];
	int32_t ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		a = b = buf;
		for (b = buf; *b && !isspace(*b); b++); *b++ = '\0';
		for (c = b;   *c && !isspace(*c); c++); *c++ = '\0';
		char *dev = a, *path = b;

		if (hash_get(&seen, path))
			continue;
		hash_set(&seen, path, "1");

		if (lstat(path, &st) != 0
		 || statvfs(path, &fs) != 0
		 || !major(st.st_dev))
			continue;

		printf("KEY %s:fs:%s %s\n",  PREFIX, path, dev);
		printf("KEY %s:dev:%s %s\n", PREFIX, dev, path);

		printf("SAMPLE %i %s:df:%s:inodes.total %lu\n", ts, PREFIX, path, fs.f_files);
		printf("SAMPLE %i %s:df:%s:inodes.free %lu\n",  ts, PREFIX, path, fs.f_favail);
		printf("SAMPLE %i %s:df:%s:inodes.rfree %lu\n", ts, PREFIX, path, fs.f_ffree - fs.f_favail);

		printf("SAMPLE %i %s:df:%s:bytes.total %lu\n", ts, PREFIX, path, fs.f_frsize *  fs.f_blocks);
		printf("SAMPLE %i %s:df:%s:bytes.free %lu\n",  ts, PREFIX, path, fs.f_frsize *  fs.f_bavail);
		printf("SAMPLE %i %s:df:%s:bytes.rfree %lu\n", ts, PREFIX, path, fs.f_frsize * (fs.f_bfree - fs.f_bavail));
#if 0
		fprintf(stderr,
			"f_bsize   = %lu\n"
			"f_frsize  = %lu\n"
			"f_blocks  = %lu\n"
			"f_bfree   = %lu\n"
			"f_bavail  = %lu\n"
			"f_bavail  = %lu\n"
			"f_files   = %lu\n"
			"f_ffree   = %lu\n"
			"f_favail  = %lu\n"
			"f_fsid    = %lu\n"
			"f_flag    = %lu\n"
			"f_namemax = %lu\n\n",
			fs.f_bsize, fs.f_frsize, fs.f_blocks, fs.f_bfree,
			fs.f_bavail, fs.f_files, fs.f_ffree, fs.f_favail,
			fs.f_fsid, fs.f_flag, fs.f_namemax);

		struct statvfs {
			unsigned long  f_bsize;    /* filesystem block size */
			unsigned long  f_frsize;   /* fragment size */
			fsblkcnt_t     f_blocks;   /* size of fs in f_frsize units */
			fsblkcnt_t     f_bfree;    /* # free blocks */
			fsblkcnt_t     f_bavail;   /* # free blocks for unprivileged users */
			fsfilcnt_t     f_files;    /* # inodes */
			fsfilcnt_t     f_ffree;    /* # free inodes */
			fsfilcnt_t     f_favail;   /* # free inodes for unprivileged users */
			unsigned long  f_fsid;     /* filesystem ID */
			unsigned long  f_flag;     /* mount flags */
			unsigned long  f_namemax;  /* maximum filename length */
		};
#endif

	}

	fclose(io);
	return 0;
}
