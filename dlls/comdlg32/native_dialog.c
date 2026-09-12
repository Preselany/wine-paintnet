/* Paint.NET Ubuntu native dialog bridge. LGPL-2.1-or-later. */
#if 0
#pragma makedep unix
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ntstatus.h"
#include "windef.h"
#include "winternl.h"
#include "native_dialog.h"

extern char **environ;

static NTSTATUS native_dialog(void *args)
{
    struct native_dialog_params *p = args;
    const char *helper = getenv("WINE_NATIVE_FILE_DIALOG");
    char interpreter[] = "/usr/bin/python3";
    char *argv[] = {interpreter, (char *)helper, NULL};
    posix_spawn_file_actions_t actions;
    char temporary[] = "/tmp/paintnet-dialog-XXXXXX";
    int input, output[2], status, error;
    unsigned int used = 0;
    ssize_t size;
    pid_t child;
    NTSTATUS ret = STATUS_UNSUCCESSFUL;
    struct pollfd poll_fd;
    BOOL read_failed = FALSE;

    if (!helper || helper[0] != '/' || access(helper, R_OK)) return STATUS_NOT_SUPPORTED;
    if (!p->request_size || p->request_size > NATIVE_DIALOG_LIMIT) return STATUS_INVALID_PARAMETER;
    /* An unlinked private file avoids pipe deadlocks when a request has many filters. */
    if ((input = mkstemp(temporary)) < 0) return ret;
    unlink(temporary);
    fcntl(input, F_SETFD, FD_CLOEXEC);
    while (used < p->request_size)
    {
        size = write(input, p->request + used, p->request_size - used);
        if (size < 0 && errno == EINTR) continue;
        if (size <= 0) goto close_input;
        used += size;
    }
    if (lseek(input, 0, SEEK_SET) < 0 || pipe2(output, O_CLOEXEC)) goto close_input;
    if (posix_spawn_file_actions_init(&actions)) goto close_output;
    error = posix_spawn_file_actions_adddup2(&actions, input, STDIN_FILENO);
    if (!error) error = posix_spawn_file_actions_adddup2(&actions, output[1], STDOUT_FILENO);
    if (!error) error = posix_spawn(&child, argv[0], &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error) goto close_output;
    close(output[1]);
    output[1] = -1;
    used = 0;
    poll_fd.fd = output[0];
    poll_fd.events = POLLIN;
    while (used < NATIVE_DIALOG_LIMIT)
    {
        if (__atomic_load_n(&p->cancelled, __ATOMIC_ACQUIRE))
        {
            /* Only terminate the child spawned for this particular dialog. */
            kill(child, SIGKILL);
            ret = STATUS_CANCELLED;
            break;
        }
        error = poll(&poll_fd, 1, 100);
        if (error < 0 && errno == EINTR) continue;
        if (error < 0) { read_failed = TRUE; break; }
        if (!error) continue;
        size = read(output[0], p->response + used, NATIVE_DIALOG_LIMIT - used);
        if (size < 0 && errno == EINTR) continue;
        if (size < 0) read_failed = TRUE;
        if (size <= 0) break;
        used += size;
    }
    if (used == NATIVE_DIALOG_LIMIT || read_failed) kill(child, SIGKILL);
    /* The helper uses the same limit. Reject truncation instead of accepting a partial path. */
    close(output[0]);
    output[0] = -1;
    for (;;)
    {
        error = waitpid(child, &status, WNOHANG);
        if (error < 0 && errno == EINTR) continue;
        if (error) break;
        if (__atomic_load_n(&p->cancelled, __ATOMIC_ACQUIRE))
        {
            kill(child, SIGKILL);
            ret = STATUS_CANCELLED;
        }
        poll(NULL, 0, 100);
    }
    if (ret != STATUS_CANCELLED && !read_failed && error > 0 && WIFEXITED(status) && !WEXITSTATUS(status) &&
        used && used < NATIVE_DIALOG_LIMIT && !p->response[used - 1])
    {
        p->response_size = used;
        ret = STATUS_SUCCESS;
    }
close_output:
    if (output[0] != -1) close(output[0]);
    if (output[1] != -1) close(output[1]);
close_input:
    close(input);
    return ret;
}

const unixlib_entry_t __wine_unix_call_funcs[] = {native_dialog};
