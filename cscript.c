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

// returns the short name of the program.
// e.g. if located at /usr/local/bin/cinterp, returns cinterp
const char *get_program_name()
{
	static const char *result = NULL;
	if (result)
		return result;

#if defined(__APPLE__) || defined(BSD)
	extern const char *getprogname();
	result = getprogname();
#elif defined(__GLIBC__)
	extern const char *program_invocation_short_name;
	result = program_invocation_short_name;
#else
#error Unknown target. Please update get_program_name() with your OS + libc version!
#endif

	return result;
}

// Variant of dirname(3) which is guaranteed to never modify arguments.
// This guarantee is not made by POSIX.
char *xdirname(const char *path)
{
	char *dup = strdup(path);
	if (!dup)
		err(EX_SOFTWARE, "failed to duplicate path: strdup");

	return dirname(dup);
}

// return index of source path or -1 on failure
int find_source_path_idx(int argc, char **argv)
{
	const char *name = get_program_name();

	// try finding file argument with shebang
	for (int i = 1; i < argc; ++i) {
		int fd = open(argv[i], O_RDONLY);
		if (fd < 0)
			continue;

		char line[512];
		ssize_t nread = read(fd, line, sizeof(line));
		if (nread <= 0) {
			close(fd);
			continue;
		}

		if (strstr(line, name) != NULL) {
			close(fd);
			errno = 0; // reset after failed attempts to open
			return i;
		}

		close(fd);
	}

	// try finding just a file
	for (int i = 1; i < argc; ++i) {
		if (access(argv[i], F_OK) == 0) {
			return i;
		}
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

char *get_executable_path(const char *source_path)
{
	char cache_path[PATH_MAX] = { };

	// get cache directory
	const char *cache_dir = getenv("XDG_CACHE_HOME");
	if (!cache_dir)
		cache_dir = "/tmp";

	// get source file
	char *abs_source_path = realpath(source_path, NULL);
	if (!abs_source_path)
		err(EX_OSERR, "cannot get real path of source file: realpath");
	for (char *s = abs_source_path; *s; ++s)
		if (*s == '/')
			*s = '%';

	// construct path
	if (snprintf(cache_path, sizeof(cache_path), "%s/%s/%s", cache_dir,
				get_program_name(), abs_source_path) >= sizeof(cache_path))
		err(EX_SOFTWARE, "final path to cached executable too long: snprintf");


	// ensure parent directory exists
	char *dirname = xdirname(cache_path);
	if (mkdirp(dirname, 0777) < 0)
		err(EX_OSERR, "failed to create cache directory (%s): mkdirp", cache_dir);
	free(dirname);

	// technically a leak but it doesn't matter since it's one time and
	// we're exec()ing another process anyways
	char *dup = strdup(cache_path);
	if (!dup)
		err(EX_SOFTWARE, "failed to duplicate cache path: strdup");

	return dup;
}

void compile_executable(char *out_path, char *source_path, char **flags, int nflags)
{
	// ensure compilation is needed
	struct stat source_stat;
	struct stat cache_stat;
	if (stat(source_path, &source_stat) >= 0 && stat(out_path, &cache_stat) >= 0)
		if (source_stat.st_mtime < cache_stat.st_mtime)
			return;

	// construct argument list from flags and stuff
	// don't look - this isn't pretty
	char *args[10 + nflags];
	int i = 0;
	args[i++] = "cc";
	args[i++] = "-o";
	args[i++] = out_path;
        args[i++] = "-x"; // CC cannot guess language from extension when using stdin
	args[i++] = "c";
	args[i++] = "-D__CSCRIPT__"; // let applications know they're running under cscript
	args[i++] = "-I"; // make relative includes work as if a filename (not -) had been passed
	args[i++] = xdirname(source_path);
	memcpy(&args[i], flags, nflags * sizeof(char *)); // user's flags must be last to overwrite defaults!
	i += nflags;
	args[i++] = "-"; // use stdin as source. must be super last so user flags can contain -x!!
	args[i] = NULL;

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
		perror("forked compilation process failed to exec: execvp");
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
		char *line = NULL;
		size_t linecap = 0;
		ssize_t linelen;
		bool first_line = true;
		while ((linelen = getline(&line, &linecap, source)) > 0) {
			if (first_line && linelen > 2 && line[0] == '#' && line[1] == '!')
				continue;

			if (fwrite(line, linelen, 1, compilation) < 1)
				err(EX_OSERR, "failed to write to write-end of pipe for compilation process: fwrite");
			first_line = false;
		}
		if (linelen == -1 && !feof(source))
			err(EX_OSERR, "failed to read source file (%s): getline", source_path);
		free(line);
		fclose(compilation);
		fclose(source);

		// wait for child to exit and review exit code
		int stat_loc;
		wait(&stat_loc);
		if (!WIFEXITED(stat_loc) || WEXITSTATUS(stat_loc) != EXIT_SUCCESS)
			errx(EX_SOFTWARE, "compilation failed. see output above.");
	}
}

// exec(1)'s `name` with `flags`
noreturn void run_executable(char *execuable_path, char **flags, int nflags, bool debug)
{
	// assemble args for final executable
	// if -g is specified, run under lldb
	// FIXME: lldb doesn't load dSYM files. differing UUIDs??
	char *executable;
	char *args[nflags+7];
	if (debug) {
		executable = "lldb";

		args[0] = "lldb";
		args[1] = "--batch";
		args[2] = "--source-quietly";
		args[3] = "--one-line";
		args[4] = "run";
		args[5] = execuable_path;
		memcpy(&args[6], flags, nflags * sizeof(char *));
		args[6+nflags] = NULL;
	} else {
		executable = execuable_path;

		memcpy(&args[0], flags, nflags * sizeof(char *));
		args[nflags] = NULL;
	}

	execvp(executable, args);
	err(EX_OSERR, "failed to exec() final executable (%s): execvp", executable);
}

bool contains_dash_g(char *source_path, char **flags, int nflags)
{
	for (int i = 0; i < nflags; ++i) {
		if (strncmp(flags[i], "-g", 2) == 0) {
			return true;
		}
	}

	return false;
}

int main(int argc, char **argv)
{
	int i = find_source_path_idx(argc, argv);
	if (i < 0)
		errx(EX_USAGE, "cannot find file to be executed in arguments");

	char *executable_path = get_executable_path(argv[i]);
	compile_executable(executable_path, argv[i], argv + 1, i - 1);
	bool want_debug = contains_dash_g(argv[i], argv + 1, i - 1);
	run_executable(executable_path, argv + i, argc - i, want_debug);
}
