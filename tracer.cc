#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__apple__)
#define PTRACE_TRACEME PT_TRACE_ME
#define PT_STEP PTRACE_SINGLESTEP
#endif

#ifdef __x86_64__
#define BP regs.rbp
#define SP regs.rsp
#define IP regs.rip
#else
#define BP regs.ebp
#define SP regs.esp
#define IP regs.eip
#endif

long checked_ptrace(enum __ptrace_request req, pid_t pid, long addr, long data, const char* req_str, const char* file, int line) {
    long r = ptrace(req, pid, addr, data);
    if (r == -1) {
        fprintf(stderr, "%s:%d: ptrace(%s): ", file, line, req_str);
        perror("");
        // exit(1);
    }
    return r;
}

#define CHECKED_PTRACE(req, pid, addr, data) checked_ptrace(req, pid, addr, data, #req, __FILE__, __LINE__)

int main(int argc, char* argv[]) {
    argc--;
    argv++;

    pid_t pid;
    if (!strcmp(argv[0], "-p")) {
        pid = strtol(argv[1], 0, 10);
        CHECKED_PTRACE(PTRACE_ATTACH, pid, 0, 0);
    } else {
        pid = fork();
        if (!pid) {
            CHECKED_PTRACE(PTRACE_TRACEME, 0, 0, 0);
            execv(argv[0], argv);
            abort();
        }
    }

    long prev = 0;
    while (true) {
        int status;
        wait(&status);
        if (!WIFSTOPPED(status)) break;

        struct user regs;
        CHECKED_PTRACE(PTRACE_GETREGS, pid, 0, (long)&regs);
        if (prev != regs.IP) printf("%p\n", (void*)regs.IP);
        prev = regs.IP;

        CHECKED_PTRACE(PTRACE_SINGLESTEP, pid, 0, 0);
    }
}
