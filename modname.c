// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2022 Stefan Lengfeld

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>

// NOTES:
// * memory allocation errors are hard failures. The program just exists.

// assert.h replacement
// - without NDEBUG misfeature
// - exit() instead of abort. No coredump wanted.
#define ASSERT(expr)                                                           \
	do {                                                                   \
		if ((bool)(expr) == false) {                                   \
			fprintf(stderr, "%s:%d: %s: Assertion `%s' failed.\n", \
				__FILE__, __LINE__, __func__, #expr);          \
			exit(EX_SOFTWARE);                                     \
		}                                                              \
	} while (false)

// returns -1 on error, returns 0 on success
// Print string to stdout via readline functions.
int stuff_string(const char *str) {
	while (*str != '\0') {
		int ret = rl_stuff_char(*str);
		if (ret != 1)
			return -1;
		str++;
	}
	return 0;
}

void remove_trailing_slashes(char *const str) {
	int pos = strlen(str) - 1;
	while (str[pos] == '/' && pos >= 0) {
		str[pos] = '\0';
		pos--;
	}
}

typedef struct dir_filename {
	char *directory;
	char *filename;
} dir_filename_t;
dir_filename_t get_filename_from_path(const char *const str) {
	int pos = strlen(str) - 1;
	while (str[pos] != '/' && pos >= 0) {
		pos--;
	}
	if (pos < 0)
		pos = 0;
	if (str[pos] == '/')
		pos += 1;

	const char *filename = &str[pos];
	char *filename_malloced = strdup(filename);
	ASSERT(filename_malloced != NULL);

	char *directory_malloced;
	if (pos == 0) {
		// No directory
		directory_malloced = strdup("");
	} else {
		if (pos > 0)
			pos--;
		directory_malloced = malloc(pos + 1);
		memcpy(directory_malloced, str, pos);
		directory_malloced[pos] = '\0';
	}
	ASSERT(directory_malloced != NULL);

	dir_filename_t ret = {directory_malloced, filename_malloced};
	// printf("xx: %s '%s' '%s'\n", str, directory_malloced,
	// filename_malloced );

	return ret;
}

void free_dir_filename(dir_filename_t *x) {
	free(x->directory);
	free(x->filename);
	x->directory = NULL;
	x->filename = NULL;
}

// Requirements:
// * b does not start with a slash!
// * a does not end with a slash!
char *path_concat(const char *a, const char *b) {
	size_t len_a = strlen(a);
	size_t len_b = strlen(b);
	if (len_a == 0) {
		return strdup(b);
	}

	ASSERT(a[len_a - 1] != '/');

	char *buffer = malloc(len_a + len_b + 2);
	buffer[0] = 0;
	strcat(buffer, a);
	if (len_b != 0) {
		strcat(buffer, "/");
		strcat(buffer, b);
	}
	return buffer;
}

// Inspired from the systemd codebase.
// A 'RAII' demalloc/free for C.
static inline void freep(void *p) {
	free(*(void **)p);
}
#define _cleanup_free_ __attribute__((__cleanup__(freep)))

int interactive_rename(const char *ro_oldpath) {
	_cleanup_free_ char *oldpath = strdup(ro_oldpath);
	_cleanup_free_ char *newpath = NULL;
	_cleanup_free_ char *newfilename = NULL;
	int ret;
	ASSERT(oldpath != NULL);

	// Remove trailing slashes
	remove_trailing_slashes(oldpath);

	if (strlen(oldpath) == 0) {
		return 0; // Skipping empty arguments
	}

	dir_filename_t dir_filename = get_filename_from_path(oldpath);
	if (strlen(dir_filename.filename) == 0) {
		fprintf(stderr, "Filename is empty!\n");
		ret = -1;
		goto free_dir_filename;
	}

	const char *oldfilename = dir_filename.filename;

	// Print old filename into readline buffer for editing.  The buffer can
	// only take 512 characters. But most filesystems only supports
	// filenames up to 255 characters anyway.
	ret = stuff_string(oldfilename);
	if (ret != 0) {
		fprintf(stderr, "Filename too long!\n");
		ret = -1;
		goto free_dir_filename;
	}

	// Display prompt with old filename and read new filename back.
	newfilename = readline("> ");

	// TODO when does this happen?
	if (newfilename == NULL) {
		fprintf(stderr, "newfilename is NULL\n");
		exit(1);
	}

	if (strlen(newfilename) == 0) {
		printf("New filename is empty. Skipping file!\n");
		ret = 0;
		goto free_dir_filename;
	}

	if (strstr(newfilename, "/") != NULL) {
		fprintf(stderr, "New filename cannot contain a slash.\n");
		ret = -1;
		goto free_dir_filename;
	}

	// Add path to readline history
	add_history(newfilename);

	newpath = path_concat(dir_filename.directory, newfilename);

	// Do stuff the actual rename
	ret = rename(oldpath, newpath);
	if (ret < 0) {
		fprintf(stderr, "Cannot rename file '%s': %s\n", oldpath,
			strerror(errno));
	}

free_dir_filename:
	free_dir_filename(&dir_filename);
	return ret;
}

#ifndef TEST
// Based on https://en.wikipedia.org/wiki/GNU_Readline
int main(int argc, const char **argv) {
	ASSERT(argc >= 1);

	// Configure readline to auto-complete paths when the tab key is hit.
	rl_bind_key('\t', rl_complete);

	ASSERT(*argv != NULL);

	bool was_error = false;

	while (*(++argv) != NULL) {
		const char *path = *argv;

		int ret;
		ret = interactive_rename(path);
		if (ret < 0) {
			was_error = true;
			break;
		}
	}

	if (was_error) {
		return 1; // There was at least on error
	}
	return 0;
}
#else

// Self made ad-hoc testing framework. Very limited.
#define ASSERT_STREQ(a, b)                                                     \
	do {                                                                   \
		if (strcmp(a, b) != 0) {                                       \
			fprintf(stderr, "%s:%d: %s: string `%s' != '%s'!\n",   \
				__FILE__, __LINE__, __func__, (a), (b));       \
			exit(EX_SOFTWARE);                                     \
		}                                                              \
	} while (false)

void test_get_filename_from_path(const char *path, const char *directory,
				 const char *filename) {
	dir_filename_t x = get_filename_from_path(path);
	ASSERT_STREQ(x.directory, directory);
	ASSERT_STREQ(x.filename, filename);
	free_dir_filename(&x);
}

void test_path_concat(const char *a, const char *b, const char *result) {
	_cleanup_free_ char *y = path_concat(a, b);
	ASSERT_STREQ(y, result);
}

int main(int argc, const char **argv) {
	test_get_filename_from_path("dir/dir/file", "dir/dir", "file");
	test_get_filename_from_path("/file", "", "file");
	test_get_filename_from_path("file", "", "file");
	test_get_filename_from_path("dir/", "dir", "");

	test_path_concat("", "file", "file");
	test_path_concat("dir", "file", "dir/file");
	test_path_concat("dir/dir", "file", "dir/dir/file");
	test_path_concat("dir/dir", "", "dir/dir");

	printf("All tests successful.\n");
}
#endif
