#define _POSIX_C_SOURCE_1

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#ifdef __APPLE__
#define st_mtime st_mtimespec.tv_sec
#endif

#ifndef NAME
#warning NAME was not defined. NAME should be defined on the commandline or by the makefile. Defaulting to "cscript".
#define NAME "cscript"
#endif

// returns index of first executable in argc or -1 on failure
int find_executable(int argc, char **argv)
{
	for (int i = 0; i < argc; ++i) {
		int fd = open(argv[i], O_RDONLY);
		if (fd < 0)
			continue;

		char line[512];
		ssize_t nread = read(fd, line, sizeof(line));
		if (nread <= 0) {
			close(fd);
			continue;
		}

		if (strstr(line, NAME) != NULL) {
			close(fd);
			errno = 0; // reset after failed attempts to open
			return i;
		}

		close(fd);
	}

	errno = 0; // reset after failed attempts to open
	return -1;
}

// C equivalent to `mkdir -p`; stolen from DOOM engine
int mkdirp(const char *path, mode_t mode)
{
	char *pathname = NULL;
	char *parent = NULL;

	if (NULL == path)
		return -1;

	pathname = strdup(path);
	if (NULL == pathname)
		goto fail;

	parent = strdup(pathname);
	if (NULL == parent)
		goto fail;

	char *p = parent + strlen(parent);
	while ('/' != *p && p != parent)
		p--;
	*p = '\0';

	// make parent dir
	if (p != parent && 0 != mkdirp(parent, mode))
		goto fail;
	free(parent);

	// make this one if parent has been made
	int rc = mkdir(pathname, mode);

	free(pathname);

	return (rc == 0 || errno == EEXIST)
		? 0
		: -1;

fail:
	free(pathname);
	free(parent);
	return -1;
}

char *get_executable_cache_path(const char *source_path)
{
	char cache_path[PATH_MAX] = { };

	// get cache directory
	char *cache_dir = getenv("XDG_CACHE_HOME");
	if (!cache_dir)
		cache_dir = "/tmp";

	// ensure exists
	if (mkdirp(cache_dir, 0777) < 0)
		err(EX_OSERR, "failed to create cache directory (%s): mkdirp", cache_dir);

	// get source file
	char *abs_source_path = realpath(source_path, NULL);
	if (!abs_source_path)
		err(EX_OSERR, "cannot get real path of source file: realpath");
	for (char *s = abs_source_path; *s; ++s)
		if (*s == '/')
			*s = '%';

	// construct path
	if (snprintf(cache_path, sizeof(cache_path), "%s/%s/%s", cache_dir,
				NAME, abs_source_path) >= sizeof(cache_path))
		err(EX_SOFTWARE, "final path to cached executable too long");

	// technically a leak but it doesn't matter since it's one time and
	// we're exec()ing another process anyways
	char *dup = strdup(cache_path);
	if (!dup)
		err(EX_SOFTWARE, "failed to duplicate cache path: strdup");

	return dup;
}

void compile_executable(char *cache_path, char *source_path, char **flags, int nflags)
{
	// ensure compilation is needed
	struct stat source_stat;
	struct stat cache_stat;
	if (stat(source_path, &source_stat) >= 0 && stat(cache_path, &cache_stat) >= 0)
		if (source_stat.st_mtime < cache_stat.st_mtime)
			return;

	// construct argument list from flags and stuff
	// don't look - this isn't pretty
	char *args[8+nflags];
	args[0] = "cc";
	args[1] = "-o";
	args[2] = cache_path;
	args[3] = "-D__CSCRIPT__";
	args[4] = "-x";
	args[5] = "c";
	memcpy(&args[6], flags, nflags * sizeof(char *));
	args[6+nflags] = "-";
	args[7+nflags] = NULL;

	// create pipe for communincation with child
	int pdes[2]; // read, write
	if (pipe(pdes) < 0)
		err(EX_OSERR, "failed to create pipe for compilation process: pipe");

	pid_t child_pid = fork();
	if (child_pid < 0) {         // error (parent)
		err(EX_OSERR, "failed to fork compilation process: fork");
	} else if (child_pid == 0) { // child
		if (dup2(pdes[0], STDIN_FILENO) < 0)
			err(EX_OSERR, "failed to duplicate pipe for compilation process: dup2");
		close(pdes[1]);

		execvp(args[0], args);
		perror("child process failed to exec");
		_exit(127);
	} else {                     // parent
		close(pdes[0]);

		FILE *compilation = fdopen(pdes[1], "w");
		if (!compilation)
			err(EX_OSERR, "failed to open write-end of pipe for compilation process: fopen");
		FILE *source = fopen(source_path, "r");
		if (!source)
			err(EX_OSERR, "failed to open source file: fopen");

		// copy all lines but the first from source file to child process's stdin
		// FIXME: this makes __FILE__ = "<stdin>" which makes a lot of
		// macros and compiler messages act weird
		char buf[1024];
		for (bool first_line = true; fgets(buf, sizeof(buf), source); first_line = false)
			if (!first_line)
				fwrite(buf, strlen(buf), 1, compilation);
		fclose(compilation);
		fclose(source);

		// wait for child to exit and review exit code
		int stat_loc;
		wait(&stat_loc);
		if (!WIFEXITED(stat_loc) || WEXITSTATUS(stat_loc) != EXIT_SUCCESS)
			errx(EX_SOFTWARE, "compilation failed. see output above.");
	}
}

// exec(1)'s name with `flags`
noreturn void run_executable(char *name, char **flags, int nflags)
{
	// assemble args for final executable
	char *args[1+nflags];
	memcpy(&args[0], flags, nflags * sizeof(char *));
	args[0+nflags] = NULL;

	// NOTE: program is invoked with argv[0] == name, NOT the actual name
	//       of the executable. For example: argv[0]==./my_script rather
	//       than argv[0]==~/.cache/cscript/%path%to%my_script
	//       Use argv[0] instead of name if you want the other behavior
	execvp(name, args);
	err(EX_OSERR, "failed to exec() final executable (%s)", name);
}

int main(int argc, char **argv)
{
#ifndef NDEBUG
	const char *invocation_name = basename(argv[0]);
	if (strstr(invocation_name, NAME) == NULL)
		errx(EX_CONFIG, "invocation name (%s) does not contain `%s'. Note that you cannot rename the executable after it has been compiled; you must recompile it with a different name (see Makefile).", invocation_name, NAME);
#endif

	int i = find_executable(argc, argv);
	if (i < 0)
		errx(EX_USAGE, "cannot find executable file with %s shebang in arguments", NAME);

	char *executable_path = get_executable_cache_path(argv[i]);
	compile_executable(executable_path, argv[i], argv + 1, i - 1);
	run_executable(executable_path, argv + i, argc - i);
}
