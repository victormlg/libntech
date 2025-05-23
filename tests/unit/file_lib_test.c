/*
  Copyright 2024 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <test.h>

#include <definitions.h>
#include <file_lib.h>
#include <stdbool.h>

/* The unit tests are failing unless I include this file. I'm not smart enough
 * to figure out why. Previously we used to compile this file as well as
 * linking with libutils. However, this is problematic when subdir-objects is
 * enabled in configure.ac. This is because it breaks the make distclean
 * target on the Debian 9 bootstrap host. */
#include "../../libutils/file_lib.c"

#define TEMP_DIR "/tmp/file_lib_test"
#define TEST_FILE "file_lib_test.txt"
#define TEST_LINK "file_lib_test.link"
#define TEST_DIR "file_lib_test.dir"
#define TEST_SUBDIR "file_lib_test.sub"
#define TEST_SUBSUBDIR "file_lib_test.sub/sub"
#define TEST_STRING "BLUE balloon"
#define TEST_SUBSTRING "YELLOW balloon"
#define TEST_SUBSUBSTRING "RED balloon"

// These are just a way to pass parameters into switch_symlink_hook().
// Since it can be called from CFEngine code, we need to do it like this.
// The way COUNTDOWN works is that it counts down towards zero for each
// component in the path passed to safe_open(). When it reaches zero,
// the symlink will be inserted at that moment.
int TEST_SYMLINK_COUNTDOWN = 0;
const char *TEST_SYMLINK_NAME = "";
const char *TEST_SYMLINK_TARGET = "";
// If this is true, when the countdown has been reached, we alternate
// between deleting and creating the link. This is to test the race condition
// when creating files. Defaults to false.
bool TEST_SYMLINK_ALTERNATE = false;

static int ORIG_DIR = -1;

void switch_symlink_hook(void)
{
    if (--TEST_SYMLINK_COUNTDOWN <= 0) {
        if (TEST_SYMLINK_COUNTDOWN == 0
            || (TEST_SYMLINK_ALTERNATE && (TEST_SYMLINK_COUNTDOWN & 1)))
        {
            rmdir(TEST_SYMLINK_NAME);
            unlink(TEST_SYMLINK_NAME);
        }
        if (TEST_SYMLINK_COUNTDOWN == 0
            || (TEST_SYMLINK_ALTERNATE && !(TEST_SYMLINK_COUNTDOWN & 1)))
        {
            assert_int_equal(symlink(TEST_SYMLINK_TARGET, TEST_SYMLINK_NAME), 0);
            // If we already are root, we must force the link to be non-root,
            // otherwise the test may have no purpose.
            if (getuid() == 0)
            {
                // 100 exists in most installations, but it doesn't really matter.
                assert_int_equal(lchown(TEST_SYMLINK_NAME, 100, 100), 0);
            }
        }
    }
}

static void complain_missing_sudo(const char *function)
{
    printf("WARNING!!! %s will not run without root privileges.\n"
           "Tried using sudo with no luck.\n", function);
}

static void chdir_or_exit(const char *path)
{
    if (chdir(path) < 0)
    {
        // Don't risk writing into folders we shouldn't. Just bail.
        exit(EXIT_FAILURE);
    }
}

static void save_test_dir(void)
{
    ORIG_DIR = open(".", O_RDONLY);
    assert_true(ORIG_DIR >= 0);
}

static void close_test_dir(void)
{
    close(ORIG_DIR);
}

static void clear_tempfiles(void)
{
    unlink(TEMP_DIR "/" TEST_FILE);
    unlink(TEMP_DIR "/" TEST_LINK);
    unlink(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE);
    unlink(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE);
    rmdir(TEMP_DIR "/" TEST_SUBSUBDIR);
    rmdir(TEMP_DIR "/" TEST_SUBDIR);
    rmdir(TEMP_DIR);
}

static void setup_tempfiles(void)
{
    clear_tempfiles();

    mkdir(TEMP_DIR, 0755);
    chdir_or_exit(TEMP_DIR);
    mkdir(TEST_SUBDIR, 0755);
    mkdir(TEST_SUBSUBDIR, 0755);
    int fd = open(TEMP_DIR "/" TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int result = write(fd, TEST_STRING, strlen(TEST_STRING));
    close(fd);
    fd = open(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    result = write(fd, TEST_SUBSTRING, strlen(TEST_SUBSTRING));
    close(fd);
    fd = open(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    result = write(fd, TEST_SUBSUBSTRING, strlen(TEST_SUBSUBSTRING));
    close(fd);

    if (getuid() == 0)
    {
        // 100 exists in most installations, but it doesn't really matter.
        result = chown(TEMP_DIR "/" TEST_FILE, 100, 100);
        result = chown(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE, 100, 100);
        result = chown(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, 100, 100);
        result = chown(TEMP_DIR "/" TEST_SUBDIR, 100, 100);
        result = chown(TEMP_DIR "/" TEST_SUBSUBDIR, 100, 100);
    }

    (void)result;

    TEST_SYMLINK_ALTERNATE = false;
}

static void return_to_test_dir(void)
{
    if (fchdir(ORIG_DIR) < 0)
    {
        // Don't risk writing into folders we shouldn't. Just bail.
        exit(EXIT_FAILURE);
    }
}

static void check_contents(int fd, const char *str)
{
    char buf[strlen(str) + 1];
    assert_int_equal(read(fd, buf, strlen(str)), strlen(str));
    buf[strlen(str)] = '\0';
    assert_string_equal(buf, str);
}

static void test_safe_open_currentdir(void)
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_subdir(void)
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEST_SUBDIR "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_subsubdir(void)
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEST_SUBSUBDIR "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSUBSTRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_updir(void)
{
    setup_tempfiles();

    chdir_or_exit(TEST_SUBDIR);

    int fd;
    assert_true((fd = safe_open("../" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_upupdir(void)
{
    setup_tempfiles();

    chdir_or_exit(TEST_SUBSUBDIR);

    int fd;
    assert_true((fd = safe_open("../../" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_generic_relative_dir(void)
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEST_SUBSUBDIR "/../" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_generic_absolute_dir(void)
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEMP_DIR "/"
                                TEST_SUBDIR "/../"
                                TEST_SUBSUBDIR "/../"
                                TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_extra_slashes_relative(void)
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEST_SUBSUBDIR "//..////" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_extra_slashes_absolute(void)
{
    setup_tempfiles();

    chdir_or_exit(TEST_SUBSUBDIR);

    int fd;
    assert_true((fd = safe_open("/" TEMP_DIR "/"
                                TEST_SUBDIR "//..//"
                                TEST_SUBSUBDIR "/..//"
                                TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_unsafe_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc/passwd";
    switch_symlink_hook();

    assert_true(safe_open(TEMP_DIR "/" TEST_LINK, O_RDONLY) < 0);
    assert_int_equal(errno, ENOLINK);

    return_to_test_dir();
}

static void test_safe_open_safe_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_FILE;
    switch_symlink_hook();

    int fd;
    assert_true((fd = safe_open(TEMP_DIR "/" TEST_LINK, O_RDONLY)) >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_unsafe_inserted_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc/passwd";
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open(TEST_LINK, O_RDONLY) < 0);
    assert_int_equal(errno, ENOENT);

    return_to_test_dir();
}

static void test_safe_open_safe_inserted_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_FILE;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open(TEST_LINK, O_RDONLY) < 0);
    assert_int_equal(errno, ENOENT);

    return_to_test_dir();
}

static void test_safe_open_unsafe_switched_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = "/etc/passwd";
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open(TEST_FILE, O_RDONLY) < 0);
    assert_int_equal(errno, ENOLINK);

    return_to_test_dir();
}

static void test_safe_open_safe_switched_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 3;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    int fd;
    assert_true((fd = safe_open(TEMP_DIR "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_unsafe_dir_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc";
    switch_symlink_hook();

    assert_true(safe_open(TEMP_DIR "/" TEST_LINK "/passwd", O_RDONLY) < 0);
    assert_int_equal(errno, ENOLINK);

    return_to_test_dir();
}

static void test_safe_open_safe_dir_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEST_SUBDIR;
    switch_symlink_hook();

    int fd;
    assert_true((fd = safe_open(TEST_LINK "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_unsafe_inserted_dir_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc";
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open(TEST_LINK "/passwd", O_RDONLY) < 0);
    assert_int_equal(errno, ENOENT);

    return_to_test_dir();
}

static void test_safe_open_safe_inserted_dir_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEST_SUBDIR;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open(TEST_LINK "/" TEST_FILE, O_RDONLY) < 0);
    assert_int_equal(errno, ENOENT);

    return_to_test_dir();
}

static void test_safe_open_unsafe_switched_dir_symlink(void)
{
    setup_tempfiles();

    assert_int_equal(mkdir(TEMP_DIR "/" TEST_LINK, 0755), 0);
    if (getuid() == 0)
    {
        assert_int_equal(chown(TEMP_DIR "/" TEST_LINK, 100, 100), 0);
    }

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc";
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open(TEST_LINK "/passwd", O_RDONLY) < 0);
    assert_int_equal(errno, ENOLINK);

    return_to_test_dir();
}

static void test_safe_open_safe_switched_dir_symlink(void)
{
    setup_tempfiles();

    assert_int_equal(mkdir(TEMP_DIR "/" TEST_LINK, 0755), 0);
    if (getuid() == 0)
    {
        assert_int_equal(chown(TEMP_DIR "/" TEST_LINK, 100, 100), 0);
    }

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEST_SUBDIR;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    int fd;
    assert_true((fd = safe_open(TEST_LINK "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_create_safe_inserted_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_FILE;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    int fd = safe_open_create_perms(
        TEST_LINK, (O_RDONLY | O_CREAT), CF_PERMS_SHARED);
    assert_true(fd >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_create_alternating_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_ALTERNATE = true;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open_create_perms(TEST_LINK, O_RDONLY | O_CREAT,
                                       CF_PERMS_SHARED) < 0);
    assert_int_equal(errno, EACCES);

    return_to_test_dir();
}

static void test_safe_open_create_unsafe_switched_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = "/etc/passwd";
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open_create_perms(TEST_FILE, O_RDONLY | O_CREAT,
                                       CF_PERMS_SHARED) < 0);
    assert_int_equal(errno, ENOLINK);

    return_to_test_dir();
}

static void test_safe_open_create_switched_dangling_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = "/etc/file-that-for-sure-does-not-exist";
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open_create_perms(TEST_FILE, O_RDONLY | O_CREAT,
                                       CF_PERMS_SHARED) < 0);
    assert_int_equal(errno, EACCES);

    return_to_test_dir();
}

static void test_safe_open_create_switched_dangling_symlink_exclusively(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = "/etc/file-that-for-sure-does-not-exist";
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open_create_perms(TEST_FILE, O_WRONLY | O_CREAT | O_EXCL,
                                       CF_PERMS_SHARED) < 0);
    assert_int_equal(errno, EEXIST);

    return_to_test_dir();
}

static void test_safe_open_create_dangling_symlink_exclusively(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = "/etc/file-that-for-sure-does-not-exist";
    switch_symlink_hook();

    assert_true(safe_open_create_perms(TEST_FILE, O_WRONLY | O_CREAT | O_EXCL,
                                       CF_PERMS_SHARED) < 0);
    assert_int_equal(errno, EEXIST);

    return_to_test_dir();
}

static void test_safe_open_switched_dangling_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = "/etc/file-that-for-sure-does-not-exist";
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_true(safe_open(TEST_FILE, O_RDONLY) < 0);
    assert_int_equal(errno, ENOENT);

    return_to_test_dir();
}

static void test_safe_open_root(void)
{
    int fd;
    struct stat statbuf;
    assert_true((fd = safe_open("/", O_RDONLY)) >= 0);
    assert_int_equal(fchdir(fd), 0);
    assert_int_equal(stat("etc", &statbuf), 0);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_ending_slashes(void)
{
    setup_tempfiles();

    int fd;
    // Whether a regular file with ending slash fails to open is platform dependent,
    // so should be the same as open().
    fd = open(TEMP_DIR "/" TEST_FILE "///", O_RDONLY);
    bool ending_file_slash_ok;
    if (fd >= 0)
    {
        close(fd);
        ending_file_slash_ok = true;
    }
    else
    {
        ending_file_slash_ok = false;
    }
    fd = safe_open(TEMP_DIR "/" TEST_FILE "///", O_RDONLY);
    assert_true(ending_file_slash_ok ? (fd >= 0) : (fd < 0));
    if (fd >= 0)
    {
        close(fd);
    }
    else
    {
        assert_int_equal(errno, ENOTDIR);
    }

    assert_true((fd = safe_open(TEMP_DIR "/", O_RDONLY)) >= 0);
    close(fd);

    return_to_test_dir();
}

static void test_safe_open_null(void)
{
    setup_tempfiles();

    int fd;
    assert_false((fd = safe_open(NULL, O_RDONLY)) >= 0);
    assert_int_equal(errno, EINVAL);

    return_to_test_dir();
}

static void test_safe_open_empty(void)
{
    setup_tempfiles();

    int fd;
    assert_false((fd = safe_open("", O_RDONLY)) >= 0);
    assert_int_equal(errno, ENOENT);

    return_to_test_dir();
}

/* ***********  HELPERS ********************************************* */

static size_t GetFileSize(const char *filename)
{
    struct stat statbuf;
    int st_ret = lstat(filename, &statbuf);
    assert_int_not_equal(st_ret, -1);
    return (size_t) statbuf.st_size;
}
static void assert_file_not_exists(const char *filename)
{
    int acc_ret = access(filename, F_OK);
    assert_int_equal(acc_ret, -1);
    assert_int_equal(errno, ENOENT);
}
static void create_test_file(bool empty)
{
    unlink(TEST_FILE);

    int fd = open(TEST_FILE, O_WRONLY|O_CREAT, 0644);
    assert_int_not_equal(fd, -1);

    if (!empty)
    {
        ssize_t w_ret = write(fd, TEST_STRING, strlen(TEST_STRING));
        assert_int_equal(w_ret, strlen(TEST_STRING));
    }

    int cl_ret = close(fd);
    assert_int_not_equal(cl_ret, -1);

    assert_int_equal(GetFileSize(TEST_FILE),
                     empty ? 0 : strlen(TEST_STRING));
}

/* ****************************************************************** */

/* Make sure that opening a file with O_TRUNC always truncates it, even if
 * opening is tried several times (there is a loop in the code that resets the
 * "trunc" flag on retry, and this test simulates retrying by changing the
 * file in the middle of the operation). */
static void test_safe_open_TRUNC_safe_switched_symlink(void)
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 3;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    int fd = safe_open(TEMP_DIR "/" TEST_FILE, O_WRONLY | O_TRUNC);
    assert_int_not_equal(fd, -1);

    int cl_ret = close(fd);
    assert_int_not_equal(cl_ret, -1);

    size_t link_target_size =
        GetFileSize(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE);

    /* TRUNCATION SHOULD HAVE HAPPENED. */
    assert_int_equal(link_target_size, 0);

    return_to_test_dir();
}
static void test_safe_open_TRUNC_unsafe_switched_symlink(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 2;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    /* Since this test runs as root, we simulate an attack where the user
     * overwrites the root-owned file with a symlink. The symlink target must
     * *not* be truncated. */

    /* 1. target is owned by root */
    assert_int_equal(chown(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE, 0, 0), 0);

    /* 2. TEST, but with a user-owned symlink being injected
     * in place of the file. */
    int fd = safe_open(TEMP_DIR "/" TEST_FILE, O_WRONLY | O_TRUNC);
    assert_int_equal(fd, -1);

    size_t link_target_size =
        GetFileSize(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE);

    /* TRUNCATION MUST NOT HAPPEN. */
    assert_int_not_equal(link_target_size, 0);

    return_to_test_dir();
}

static void test_safe_open_TRUNC_existing_nonempty(void)
{
    setup_tempfiles();
    create_test_file(false);

    /* TEST: O_TRUNC */
    int fd = safe_open(TEST_FILE, O_WRONLY | O_TRUNC);
    assert_int_not_equal(fd, -1);

    int cl_ret = close(fd);
    assert_int_not_equal(cl_ret, -1);

    assert_int_equal(GetFileSize(TEST_FILE), 0);

    return_to_test_dir();
}
static void test_safe_open_TRUNC_existing_empty(void)
{
    setup_tempfiles();
    create_test_file(true);

    /* TEST: O_TRUNC */
    int fd = safe_open(TEST_FILE, O_WRONLY | O_TRUNC);
    assert_int_not_equal(fd, -1);

    int cl_ret = close(fd);
    assert_int_not_equal(cl_ret, -1);

    assert_int_equal(GetFileSize(TEST_FILE), 0);

    return_to_test_dir();
}
static void test_safe_open_TRUNC_nonexisting(void)
{
    setup_tempfiles();
    unlink(TEST_FILE);

    /* TEST: O_TRUNC */
    int fd = safe_open(TEST_FILE, O_WRONLY | O_TRUNC);
    assert_int_equal(fd, -1);
    assert_int_equal(errno, ENOENT);

    assert_file_not_exists(TEST_FILE);

    return_to_test_dir();
}
static void test_safe_open_CREAT_TRUNC_existing_nonempty(void)
{
    setup_tempfiles();
    create_test_file(false);

    /* TEST: O_CREAT | O_TRUNC */
    int fd = safe_open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC);
    assert_int_not_equal(fd, -1);

    int cl_ret = close(fd);
    assert_int_not_equal(cl_ret, -1);

    assert_int_equal(GetFileSize(TEST_FILE), 0);

    return_to_test_dir();
}
static void test_safe_open_CREAT_TRUNC_existing_empty(void)
{
    setup_tempfiles();
    create_test_file(true);

    /* TEST: O_CREAT | O_TRUNC */
    int fd = safe_open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC);
    assert_int_not_equal(fd, -1);

    int cl_ret = close(fd);
    assert_int_not_equal(cl_ret, -1);

    assert_int_equal(GetFileSize(TEST_FILE), 0);

    return_to_test_dir();
}
static void test_safe_open_CREAT_TRUNC_nonexisting(void)
{
    setup_tempfiles();
    unlink(TEST_FILE);

    /* TEST: O_TRUNC */
    int fd = safe_open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC);
    assert_int_not_equal(fd, -1);

    int cl_ret = close(fd);
    assert_int_not_equal(cl_ret, -1);

    assert_int_equal(GetFileSize(TEST_FILE), 0);

    return_to_test_dir();
}

static void test_safe_fopen(void)
{
    setup_tempfiles();

    FILE *fptr;

    char buf = 'a';

    assert_true(fptr = safe_fopen(TEST_FILE, "r"));
    assert_int_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_not_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen_create_perms(TEST_FILE, "a",
                                               CF_PERMS_DEFAULT));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen(TEST_FILE, "r+"));
    assert_int_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen_create_perms(TEST_FILE, "a+",
                                               CF_PERMS_DEFAULT));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen_create_perms(TEST_FILE, "w",
                                               CF_PERMS_DEFAULT));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen_create_perms(TEST_FILE, "w+",
                                               CF_PERMS_DEFAULT));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    unlink(TEST_FILE);
    assert_false(fptr = safe_fopen(TEST_FILE, "r"));

    unlink(TEST_FILE);
    assert_true(fptr = safe_fopen_create_perms(TEST_FILE, "a",
                                               CF_PERMS_DEFAULT));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    unlink(TEST_FILE);
    assert_true(fptr = safe_fopen_create_perms(TEST_FILE, "w",
                                               CF_PERMS_DEFAULT));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    unlink(TEST_FILE);
    assert_false(fptr = safe_fopen(TEST_FILE, "r+"));

    unlink(TEST_FILE);
    assert_true(fptr = safe_fopen_create_perms(TEST_FILE, "a+",
                                               CF_PERMS_DEFAULT));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    unlink(TEST_FILE);
    assert_true(fptr = safe_fopen_create_perms(TEST_FILE, "w+",
                                               CF_PERMS_DEFAULT));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    return_to_test_dir();
}

static void test_safe_chown_plain_file(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(chown(TEST_FILE, 100, 100), 0);
    assert_int_equal(stat(TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_chown(TEST_FILE, 0, 0), 0);
    assert_int_equal(stat(TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_chown_relative_file(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(chown(TEST_SUBSUBDIR "/" TEST_FILE, 100, 100), 0);
    assert_int_equal(stat(TEST_SUBSUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_chown(TEST_SUBSUBDIR "/" TEST_FILE, 0, 0), 0);
    assert_int_equal(stat(TEST_SUBSUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_chown_absolute_file(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(chown(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, 100, 100), 0);
    assert_int_equal(stat(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_chown(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, 0, 0), 0);
    assert_int_equal(stat(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_chown_file_extra_slashes(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(chown("/" TEMP_DIR "////" TEST_SUBSUBDIR "//" TEST_FILE, 100, 100), 0);
    assert_int_equal(stat("/" TEMP_DIR "////" TEST_SUBSUBDIR "//" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_chown("/" TEMP_DIR "////" TEST_SUBSUBDIR "//" TEST_FILE, 0, 0), 0);
    assert_int_equal(stat("/" TEMP_DIR "////" TEST_SUBSUBDIR "//" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_chown_plain_directory(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(chown(TEST_SUBDIR, 100, 100), 0);
    assert_int_equal(stat(TEST_SUBDIR, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_chown(TEST_SUBDIR, 0, 0), 0);
    assert_int_equal(stat(TEST_SUBDIR, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_chown_unsafe_link(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_int_equal(chown(TEST_SUBDIR "/" TEST_FILE, 0, 0), 0);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);
    assert_int_equal(safe_chown(TEST_FILE, 100, 100), -1);
    assert_int_equal(errno, ENOLINK);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_lchown_plain_file(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(lchown(TEST_FILE, 100, 100), 0);
    assert_int_equal(stat(TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_lchown(TEST_FILE, 0, 0), 0);
    assert_int_equal(stat(TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_lchown_relative_file(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(lchown(TEST_SUBSUBDIR "/" TEST_FILE, 100, 100), 0);
    assert_int_equal(stat(TEST_SUBSUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_lchown(TEST_SUBSUBDIR "/" TEST_FILE, 0, 0), 0);
    assert_int_equal(stat(TEST_SUBSUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_lchown_absolute_file(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(lchown(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, 100, 100), 0);
    assert_int_equal(stat(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_lchown(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, 0, 0), 0);
    assert_int_equal(stat(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_lchown_file_extra_slashes(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(lchown("/" TEMP_DIR "////" TEST_SUBSUBDIR "//" TEST_FILE, 100, 100), 0);
    assert_int_equal(stat("/" TEMP_DIR "////" TEST_SUBSUBDIR "//" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_lchown("/" TEMP_DIR "////" TEST_SUBSUBDIR "//" TEST_FILE, 0, 0), 0);
    assert_int_equal(stat("/" TEMP_DIR "////" TEST_SUBSUBDIR "//" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_lchown_plain_directory(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(lchown(TEST_SUBDIR, 100, 100), 0);
    assert_int_equal(stat(TEST_SUBDIR, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_lchown(TEST_SUBDIR, 0, 0), 0);
    assert_int_equal(stat(TEST_SUBDIR, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_lchown_unsafe_link(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_int_equal(lchown(TEST_SUBDIR "/" TEST_FILE, 0, 0), 0);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);
    // Unsafe links should succeed, because we are operating on the *link*, not the target.
    assert_int_equal(safe_lchown(TEST_FILE, 100, 100), 0);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);

    return_to_test_dir();
}

static void test_safe_lchown_unsafe_link_to_directory(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_SUBDIR;
    switch_symlink_hook();

    assert_int_equal(lchown(TEST_SUBDIR "/" TEST_FILE, 0, 0), 0);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);
    assert_int_equal(lchown(TEST_SUBDIR, 0, 0), 0);
    assert_int_equal(stat(TEST_SUBDIR, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 0);
    assert_int_equal(statbuf.st_gid, 0);
    assert_int_equal(safe_lchown(TEST_LINK "/" TEST_FILE, 100, 100), -1);
    assert_int_equal(errno, ENOLINK);

    assert_int_equal(lchown(TEST_SUBDIR "/" TEST_FILE, 100, 100), 0);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(lchown(TEST_SUBDIR, 100, 100), 0);
    assert_int_equal(stat(TEST_SUBDIR, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);
    assert_int_equal(safe_lchown(TEST_LINK "/" TEST_FILE, 100, 100), 0);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_uid, 100);
    assert_int_equal(statbuf.st_gid, 100);

    return_to_test_dir();
}

static void test_safe_chmod_plain_file(void)
{
    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(chmod(TEST_FILE, 0777), 0);
    assert_int_equal(stat(TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0777);
    assert_int_equal(safe_chmod(TEST_FILE, 0644), 0);
    assert_int_equal(stat(TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0644);

    return_to_test_dir();
}

static void test_safe_chmod_relative_file(void)
{
    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(chmod(TEST_SUBDIR "/" TEST_FILE, 0777), 0);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0777);
    assert_int_equal(safe_chmod(TEST_SUBDIR "/" TEST_FILE, 0644), 0);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0644);

    return_to_test_dir();
}

static void test_safe_chmod_absolute_file(void)
{
    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(chmod(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE, 0777), 0);
    assert_int_equal(stat(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0777);
    assert_int_equal(safe_chmod(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE, 0644), 0);
    assert_int_equal(stat(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0644);

    return_to_test_dir();
}

static void test_safe_chmod_extra_slashes(void)
{
    setup_tempfiles();

    struct stat statbuf;

    assert_int_equal(chmod("/" TEMP_DIR "///" TEST_SUBDIR "//" TEST_FILE, 0777), 0);
    assert_int_equal(stat("/" TEMP_DIR "///" TEST_SUBDIR "//" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0777);
    assert_int_equal(safe_chmod("/" TEMP_DIR "///" TEST_SUBDIR "//" TEST_FILE, 0644), 0);
    assert_int_equal(stat("/" TEMP_DIR "///" TEST_SUBDIR "//" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0644);

    return_to_test_dir();
}

static void test_safe_chmod_unsafe_link(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    struct stat statbuf;

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //switch_symlink_hook();

    assert_int_equal(chown(TEST_SUBDIR "/" TEST_FILE, 0, 0), 0);

    assert_int_equal(chmod(TEST_SUBDIR "/" TEST_FILE, 0777), 0);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0777);
    assert_int_equal(safe_chmod(TEST_FILE, 0644), -1);
    assert_int_equal(errno, ENOLINK);
    assert_int_equal(stat(TEST_SUBDIR "/" TEST_FILE, &statbuf), 0);
    assert_int_equal(statbuf.st_mode & 0777, 0777);

    return_to_test_dir();
}

static void test_safe_creat_exists(void)
{
    setup_tempfiles();

    int fd;
    struct stat buf;
    assert_true((fd = safe_creat(TEST_FILE, 0644)) >= 0);
    assert_int_equal(fstat(fd, &buf), 0);
    assert_int_equal(buf.st_size, 0);
    close(fd);

    return_to_test_dir();
}

static void test_safe_creat_doesnt_exist(void)
{
    setup_tempfiles();

    int fd;
    struct stat buf;
    unlink(TEST_FILE);
    assert_true((fd = safe_creat(TEST_FILE, 0644)) >= 0);
    assert_int_equal(fstat(fd, &buf), 0);
    assert_int_equal(buf.st_size, 0);
    close(fd);

    return_to_test_dir();
}

static void test_symlink_loop(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_FILE;
    switch_symlink_hook();

    assert_int_equal(safe_open(TEST_FILE, O_RDONLY), -1);
    assert_int_equal(errno, ELOOP);
    assert_int_equal(safe_chown(TEST_FILE, 100, 100), -1);
    assert_int_equal(errno, ELOOP);
    assert_int_equal(safe_chmod(TEST_FILE, 0644), -1);
    assert_int_equal(errno, ELOOP);
    assert_int_equal(safe_lchown(TEST_FILE, 100, 100), 0);

    return_to_test_dir();
}

static void test_safe_chmod_chown_fifos(void)
{
    if (getuid() != 0)
    {
        complain_missing_sudo(__FUNCTION__);
        return;
    }

    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = TEST_SUBDIR "/" TEST_FILE;
    switch_symlink_hook();

    unlink(TEST_SUBDIR "/" TEST_FILE);
    assert_int_equal(mkfifo(TEST_SUBDIR "/" TEST_FILE, 0644), 0);

    // Link owner != target owner
    assert_int_equal(safe_chown(TEST_FILE, 100, 100), -1);
    assert_int_equal(errno, ENOLINK);
    assert_int_equal(safe_chmod(TEST_FILE, 0755), -1);
    assert_int_equal(errno, ENOLINK);
    assert_int_equal(safe_chown(TEST_SUBDIR "/" TEST_FILE, 100, 100), 0);

    // Now the owner is correct
    assert_int_equal(safe_chmod(TEST_FILE, 0755), 0);
    assert_int_equal(safe_chown(TEST_FILE, 0, 0), 0);
    assert_int_equal(safe_chmod(TEST_SUBDIR "/" TEST_FILE, 0644), 0);

    return_to_test_dir();
}

static void test_file_can_open(void)
{
    setup_tempfiles();

    assert_true(FileCanOpen(TEST_FILE, "r"));
    assert_false(FileCanOpen("no_such_file", "r"));

    return_to_test_dir();
}

static void test_file_copy(void)
{
    setup_tempfiles();

    assert_true(File_Copy(TEST_FILE, "new_file"));

    int fd = safe_open("new_file", O_RDONLY);
    assert_true(fd >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);

    assert_int_equal(unlink("new_file"), 0);

    return_to_test_dir();
}

static void test_file_copy_to_dir(void)
{
    setup_tempfiles();

    assert_false(File_CopyToDir(TEST_FILE, "no/such/dir/"));
    assert_true(File_CopyToDir(TEST_FILE, TEST_SUBDIR "/"));

    const char * new_path = TEST_SUBDIR "/" TEST_FILE;
    int fd = safe_open(new_path, O_RDONLY);
    assert_true(fd >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);

    assert_int_equal(unlink(new_path), 0);

    return_to_test_dir();
}

static void test_file_read(void)
{
    setup_tempfiles();

    {
        bool truncated = true;
        Writer *w = FileRead(TEST_FILE, 1024, &truncated);
        assert_false(truncated);
        char *data = StringWriterClose(w);
        assert_string_equal(TEST_STRING, data);
        free(data);
    }

    {
        bool truncated = false;
        Writer *w = FileRead(TEST_FILE, 4, &truncated);
        assert_true(truncated);
        char *data = StringWriterClose(w);
        assert_string_equal("BLUE", data);
        free(data);
    }

    return_to_test_dir();
}

static void test_read_file_stream_to_buffer(void)
{
    setup_tempfiles();

    {
        const size_t length = strlen(TEST_STRING);
        char buf[1024] = {0};
        FILE *const file = safe_fopen(TEST_FILE, "r");
        assert_true(file != NULL);
        const ssize_t bytes_read = ReadFileStreamToBuffer(file, length, buf);
        fclose(file);
        assert_int_equal(bytes_read, length);
        assert_string_equal(TEST_STRING, buf);
    }

    return_to_test_dir();
}

static void test_full_read_write(void)
{
    setup_tempfiles();

    // Write test string to new_file, don't include NUL-byte
    int fd = safe_open_create_perms("new_file", O_WRONLY | O_CREAT, 0777);
    const size_t length = strlen(TEST_STRING);
    assert_int_equal(FullWrite(fd, TEST_STRING, length), length);
    assert_int_equal(close(fd), 0);

    {
        // Read the same length back from file:
        fd = safe_open("new_file", O_RDONLY);
        char buf[length];
        assert_int_equal(FullRead(fd, buf, length), length);
        assert_true(memcmp(buf, TEST_STRING, length) == 0);
        assert_int_equal(close(fd), 0);
    }

    {
        // Try to read twice as much:
        const size_t twice = length * 2;
        fd = safe_open("new_file", O_RDONLY);
        char buf[length]; // ASAN should panic if we overflow
        assert_int_equal(FullRead(fd, buf, twice), length);
        assert_true(memcmp(buf, TEST_STRING, length) == 0);
        assert_int_equal(close(fd), 0);
    }

    {
        // Read about half of the file:
        const size_t half = length / 2;
        fd = safe_open("new_file", O_RDONLY);
        char buf[half];
        assert_int_equal(FullRead(fd, buf, half), half);
        assert_true(memcmp(buf, TEST_STRING, half) == 0);
        assert_int_equal(close(fd), 0);
    }

    assert_int_equal(unlink("new_file"), 0);
    return_to_test_dir();
}

static void test_is_dir_real(void)
{
    setup_tempfiles();

    assert_true(IsDirReal("/"));
    assert_true(IsDirReal(".."));
    assert_false(IsDirReal(TEST_FILE));
    assert_false(IsDirReal("no/such/dir/"));

    // TODO: Test that IsDirReal() returns false for symlinks

    return_to_test_dir();
}

static void test_file_locking(void)
{
    /* see file_lock_test.c for some more test cases */
    setup_tempfiles();

    /** TEST CASE 1 -- open, nowait excl. lock, unlock, close **/
    int fd = open(TEMP_DIR "/" TEST_FILE, O_CREAT | O_RDWR, 0644);
    FileLock lock = EMPTY_FILE_LOCK;
    lock.fd = fd;

    /* lock without waiting */
    assert_int_equal(ExclusiveFileLock(&lock, false), 0);

    /* FD should not be changed */
    assert_int_equal(lock.fd, fd);

    /* unlock, but keep the FD open */
    assert_int_equal(ExclusiveFileUnlock(&lock, false), 0);

    /* should be able to close */
    assert_int_equal(close(lock.fd), 0);


    /** TEST CASE 2 -- open, wait excl. lock, unlock+close **/
    fd = open(TEMP_DIR "/" TEST_FILE, O_CREAT | O_RDWR, 0644);
    lock.fd = fd;

    /* lock trying to wait */
    assert_int_equal(ExclusiveFileLock(&lock, true), 0);

    /* FD should not be changed */
    assert_int_equal(lock.fd, fd);

    /* try to lock again without waiting (we already have the lock so it's a
     * no-op)*/
    assert_int_equal(ExclusiveFileLock(&lock, false), 0);

    /* unlock and close the FD */
    assert_int_equal(ExclusiveFileUnlock(&lock, true), 0);

    /* should be already closed */
    assert_int_equal(close(lock.fd), -1);

    /* FD should be reset to -1 */
    assert_int_equal(lock.fd, -1);


    /** TEST CASE 3 -- open, wait shared lock, wait excl. lock, unlock, close **/
    fd = open(TEMP_DIR "/" TEST_FILE, O_CREAT | O_RDWR, 0644);
    lock.fd = fd;

    /* SHARED lock trying to wait */
    assert_int_equal(SharedFileLock(&lock, true), 0);

    /* FD should not be changed */
    assert_int_equal(lock.fd, fd);

    /* we are holding a shared lock so WE should be able to get an exclusive
     * lock */
    assert_true(ExclusiveFileLockCheck(&lock));

    /* upgrade the lock to an exclusive one */
    assert_int_equal(ExclusiveFileLock(&lock, true), 0);

    /* unlock, but keep the FD open */
    assert_int_equal(ExclusiveFileUnlock(&lock, false), 0);

    /* should be able to close the FD */
    assert_int_equal(close(lock.fd), 0);


    /** TEST CASE 4 -- open, unlock, wait excl. lock, unlock, excl. lock again,
     *                 unlock+close **/
    fd = open(TEMP_DIR "/" TEST_FILE, O_CREAT | O_RDWR, 0644);
    lock.fd = fd;

    /* unlock, but keep the FD open (we are NOT holding the lock so this should
     * be no-op)*/
    assert_int_equal(ExclusiveFileUnlock(&lock, false), 0);

    /* FD should not be changed */
    assert_int_equal(lock.fd, fd);

    /* we should be able to get an exclusive lock */
    assert_true(ExclusiveFileLockCheck(&lock));

    /* get an exclusive lock */
    assert_int_equal(ExclusiveFileLock(&lock, true), 0);

    /* unlock, but keep the FD open */
    assert_int_equal(ExclusiveFileUnlock(&lock, false), 0);

    /* get an exclusive lock again */
    assert_int_equal(ExclusiveFileLock(&lock, true), 0);

    /* unlock and close the FD */
    assert_int_equal(ExclusiveFileUnlock(&lock, true), 0);


    return_to_test_dir();
}

static void test_file_locking_with_path(void)
{
    /* see file_lock_test.c for some more test cases */
    setup_tempfiles();

    FileLock lock = EMPTY_FILE_LOCK;

    /** TEST CASE 1 -- nowait excl. lock, unlock, close **/
    /* lock without waiting */
    assert_int_equal(ExclusiveFileLockPath(&lock, TEMP_DIR "/" TEST_FILE, false), 0);

    /* FD should be changed */
    assert_int_not_equal(lock.fd, -1);

    /* unlock, but keep the FD open */
    assert_int_equal(ExclusiveFileUnlock(&lock, false), 0);

    /* should be able to close */
    assert_int_equal(close(lock.fd), 0);
    lock.fd = -1;


    /** TEST CASE 2 -- open, wait excl. lock, unlock+close **/
    /* lock trying to wait */
    assert_int_equal(ExclusiveFileLockPath(&lock, TEMP_DIR "/" TEST_FILE, true), 0);

    /* FD should be changed */
    assert_int_not_equal(lock.fd, -1);

    /* try to lock again without waiting (we already have the lock so it's a
     * no-op)*/
    assert_int_equal(ExclusiveFileLock(&lock, false), 0);

    /* unlock and close the FD */
    assert_int_equal(ExclusiveFileUnlock(&lock, true), 0);

    /* should be already closed */
    assert_int_equal(close(lock.fd), -1);

    /* FD should be reset to -1 */
    assert_int_equal(lock.fd, -1);


    /** TEST CASE 3 -- open, wait shared lock, wait excl. lock, unlock, close **/
    /* SHARED lock trying to wait */
    assert_int_equal(SharedFileLockPath(&lock, TEMP_DIR "/" TEST_FILE, true), 0);

    /* FD should be changed */
    assert_int_not_equal(lock.fd, -1);

    /* we are holding a shared lock so WE should be able to get an exclusive
     * lock */
    assert_true(ExclusiveFileLockCheck(&lock));

    /* SharedFileLockPath opens the file as RDONLY. For an exclusive lock, we
     * need RDWR. */
    assert_int_equal(ExclusiveFileLock(&lock, true), -1);

    /* upgrade the lock to an exclusive one */
    FileLock lock2 = EMPTY_FILE_LOCK;
    assert_int_equal(ExclusiveFileLockPath(&lock2, TEMP_DIR "/" TEST_FILE, true), 0);

    /* unlock, but keep the FD open */
    assert_int_equal(ExclusiveFileUnlock(&lock, false), 0);

    /* should be able to close both FDs */
    assert_int_equal(close(lock.fd), 0);
    lock.fd = -1;
    assert_int_equal(close(lock2.fd), 0);
    lock2.fd = -1;


    /* TEST CASE 4 -- try to use lock file in non-existing directory */
    assert_int_equal(ExclusiveFileLockPath(&lock, "non-existing-dir/" TEST_FILE, true), -2);


    return_to_test_dir();
}

static void try_gaining_root_privileges(ARG_UNUSED int argc, char **argv)
{
    if (system("sudo -n /bin/true") == 0)
    {
        execlp("sudo", "sudo", "-n", argv[0], NULL);
        // Should never get here.
    }
}

int main(int argc, char **argv)
{
    if (getuid() != 0)
    {
        try_gaining_root_privileges(argc, argv);
    }

    PRINT_TEST_BANNER();

    const UnitTest tests[] =
        {
            unit_test(save_test_dir),

            unit_test(test_safe_open_currentdir),
            unit_test(test_safe_open_subdir),
            unit_test(test_safe_open_subsubdir),
            unit_test(test_safe_open_updir),
            unit_test(test_safe_open_upupdir),
            unit_test(test_safe_open_generic_relative_dir),
            unit_test(test_safe_open_generic_absolute_dir),
            unit_test(test_safe_open_extra_slashes_relative),
            unit_test(test_safe_open_extra_slashes_absolute),
            unit_test(test_safe_open_unsafe_symlink),
            unit_test(test_safe_open_safe_symlink),
            unit_test(test_safe_open_unsafe_inserted_symlink),
            unit_test(test_safe_open_safe_inserted_symlink),
            unit_test(test_safe_open_unsafe_switched_symlink),
            unit_test(test_safe_open_safe_switched_symlink),
            unit_test(test_safe_open_unsafe_dir_symlink),
            unit_test(test_safe_open_safe_dir_symlink),
            unit_test(test_safe_open_unsafe_inserted_dir_symlink),
            unit_test(test_safe_open_safe_inserted_dir_symlink),
            unit_test(test_safe_open_unsafe_switched_dir_symlink),
            unit_test(test_safe_open_safe_switched_dir_symlink),
            unit_test(test_safe_open_create_safe_inserted_symlink),
            unit_test(test_safe_open_create_alternating_symlink),
            unit_test(test_safe_open_create_unsafe_switched_symlink),
            unit_test(test_safe_open_create_switched_dangling_symlink),
            unit_test(test_safe_open_create_switched_dangling_symlink_exclusively),
            unit_test(test_safe_open_create_dangling_symlink_exclusively),
            unit_test(test_safe_open_switched_dangling_symlink),
            unit_test(test_safe_open_root),
            unit_test(test_safe_open_ending_slashes),
            unit_test(test_safe_open_null),
            unit_test(test_safe_open_empty),

            unit_test(test_safe_open_TRUNC_safe_switched_symlink),
            unit_test(test_safe_open_TRUNC_unsafe_switched_symlink),
            unit_test(test_safe_open_TRUNC_existing_nonempty),
            unit_test(test_safe_open_TRUNC_existing_empty),
            unit_test(test_safe_open_TRUNC_nonexisting),
            unit_test(test_safe_open_CREAT_TRUNC_existing_nonempty),
            unit_test(test_safe_open_CREAT_TRUNC_existing_empty),
            unit_test(test_safe_open_CREAT_TRUNC_nonexisting),

            unit_test(test_safe_fopen),

            unit_test(test_safe_chown_plain_file),
            unit_test(test_safe_chown_relative_file),
            unit_test(test_safe_chown_absolute_file),
            unit_test(test_safe_chown_file_extra_slashes),
            unit_test(test_safe_chown_plain_directory),
            unit_test(test_safe_chown_unsafe_link),

            unit_test(test_safe_lchown_plain_file),
            unit_test(test_safe_lchown_relative_file),
            unit_test(test_safe_lchown_absolute_file),
            unit_test(test_safe_lchown_file_extra_slashes),
            unit_test(test_safe_lchown_plain_directory),
            unit_test(test_safe_lchown_unsafe_link),
            unit_test(test_safe_lchown_unsafe_link_to_directory),

            unit_test(test_safe_chmod_plain_file),
            unit_test(test_safe_chmod_relative_file),
            unit_test(test_safe_chmod_absolute_file),
            unit_test(test_safe_chmod_extra_slashes),
            unit_test(test_safe_chmod_unsafe_link),

            unit_test(test_safe_creat_exists),
            unit_test(test_safe_creat_doesnt_exist),

            unit_test(test_symlink_loop),

            unit_test(test_safe_chmod_chown_fifos),

            unit_test(test_file_can_open),
            unit_test(test_file_copy),
            unit_test(test_file_copy_to_dir),
            unit_test(test_file_read),
            unit_test(test_read_file_stream_to_buffer),
            unit_test(test_full_read_write),
            unit_test(test_is_dir_real),

            unit_test(test_file_locking),
            unit_test(test_file_locking_with_path),

            unit_test(close_test_dir),
            unit_test(clear_tempfiles),
        };

    int ret = run_tests(tests);

    return ret;
}
