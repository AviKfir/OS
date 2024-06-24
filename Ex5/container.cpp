#include <iostream>
#include <sched.h>
#include <sys/wait.h>
#include <cstring>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fstream>

#define STACK 8192
#define PERMISSION_MODE 0755
#define SET_HOST_ERROR_MSG "system error: sethostname function failed\n"
#define CHROOT_ERROR_MSG "system error: chroot function failed\n"
#define CHDIR_ERROR_MSG "system error: chdir function failed\n"
#define MOUNT_ERROR_MSG "system error: mount function failed\n"
#define MKDIR_ERROR_MSG "system error: mkdir function failed\n"
#define EXECVP_ERROR_MSG "system error: execvp function failed\n"
#define CLONE_ERROR_MSG "system error: clone function failed\n"
#define REMOVE_ERROR_MSG "system error: remove function failed\n"
#define UMOUNT_ERROR_MSG "system error: umount function failed\n"
#define CHMOD_ERROR_MSG "system error: chmod function failed\n"
#define MALLOC_ERROR_MSG "malloc failed\n"

#define FAILURE 1
#define SUCCESS 0

#define PATH_RELEASE "/sys/fs/cgroup/pids/notify_on_release"
#define PATH_MAX "/sys/fs/cgroup/pids/pids.max"
#define PATH_PROCS "/sys/fs/cgroup/pids/cgroup.procs"
#define PATH_PIDS "/sys/fs/cgroup/pids"
#define PATH_CGROUPS "/sys/fs/cgroup"
#define PATH_FS "/sys/fs"

/**
 * struct for arguments from command line
 */
struct CommandLineArg {
    int argc;
    char** argv;
};

/**
 * the function for the container.
 * @param arg arguments from command line.
 * @return 0 on success, exit(-1) on failure.
 */
int child(void* arg) {
    CommandLineArg* argStruct = (CommandLineArg*) arg;
    char* name = argStruct->argv[1];
    if (sethostname(name, strlen(name)) != 0) {
        fprintf (stderr, SET_HOST_ERROR_MSG);
        exit (FAILURE);
    }

    // chroot - change root
    if (chroot(argStruct->argv[2]) != 0) {
        fprintf (stderr, CHROOT_ERROR_MSG);
        exit (FAILURE);
    }
    // chdir - change the current working directory
    if (chdir(argStruct->argv[2]) != 0) {
        fprintf (stderr, CHDIR_ERROR_MSG);
        exit (FAILURE);
    }
    //limit the number of new processes that can run within the container
    if (mkdir(PATH_FS,0755) != 0) {
        fprintf (stderr, MKDIR_ERROR_MSG);
        exit (FAILURE);
    }

    if (mkdir(PATH_CGROUPS,0755) != 0) {
        fprintf (stderr, MKDIR_ERROR_MSG);
        exit (FAILURE);
    }

    if (mkdir(PATH_PIDS,0755) != 0) {
        fprintf (stderr, MKDIR_ERROR_MSG);
        exit (FAILURE);
    }

        std::ofstream procs;
        procs.open(PATH_PROCS); // std::ofstream::open/etc. do not return.
        // If the function fails to open a file, the failbit state flag is set for the stream.
        if (chmod(PATH_PROCS, PERMISSION_MODE) == -1) {
            fprintf (stderr, CHMOD_ERROR_MSG);
            exit (FAILURE);
        }
        procs << getpid(); // getpid() = 1 (pid of child from his perspective)
        procs.close();

        // limit the number of new processes that can run within the container.
        std::ofstream max;
        max.open(PATH_MAX);
        if (chmod(PATH_MAX, PERMISSION_MODE) == -1) {
            fprintf (stderr, CHMOD_ERROR_MSG);
            exit (FAILURE);
        }
        max << argStruct->argv[3]; // argv[3] = max num_processes
        max.close();

        std::ofstream release;
        release.open(PATH_RELEASE);
        if (chmod(PATH_RELEASE, PERMISSION_MODE) == -1) {
            fprintf (stderr, CHMOD_ERROR_MSG);
            exit (FAILURE);
        }
        release << 1;
        release.close();

    // mount - future children of child will be saved in new proc
    if (mount("proc", "/proc", "proc", 0, 0) != 0) {
        fprintf (stderr, MOUNT_ERROR_MSG);
        exit (FAILURE);
    }
    // run the program
    int args_for_program = argStruct->argc - 5; // 5 non-relevant args
    char* vec[args_for_program + 2];
    vec[0] = argStruct->argv[4];
    for (int i = 1; i < args_for_program + 1; ++i) {
        vec[i] = argStruct->argv[4 + i];
    }
    vec[1 + args_for_program] = (char *) 0;
    if(execvp(argStruct->argv[4], vec) == -1) {
        fprintf (stderr, EXECVP_ERROR_MSG);
        exit (FAILURE);
    }
    return SUCCESS;
}

/**
 * this function constructs a container and runs a program within the container.
 * @param argc number of arguments from command line.
 * @param argv arguments from command line.
 * @return 0 on success, exit(-1) on failure.
 */
int main(int argc, char* argv[]) {
    int* stack = (int*) malloc(STACK);
    if (stack == nullptr) {
        fprintf (stderr, MALLOC_ERROR_MSG);
        exit (FAILURE);
    }
    *stack = *stack + STACK;
    CommandLineArg* arg = new CommandLineArg();
    arg->argc = argc;
    arg->argv = argv;
    int child_pid;
    if ((child_pid = clone(child, (void*) stack,
                           CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, (void*)arg)) == -1) {
        fprintf (stderr, CLONE_ERROR_MSG);
        exit (FAILURE);
    }
    wait(NULL);
    delete arg;

    // delete files using remove()
    std::string file_path = argv[2];
    std::string file_path_release = file_path + PATH_RELEASE;
    char* file_path_con = const_cast<char *>(file_path_release.c_str());

    if (remove(file_path_con) != 0) {
        fprintf (stderr, REMOVE_ERROR_MSG);
        exit (FAILURE);
    }

    std::string file_path_max = file_path + PATH_MAX;
    char* file_path_con1 = const_cast<char *>(file_path_max.c_str());
    if (remove(file_path_con1) != 0) {
        fprintf (stderr, REMOVE_ERROR_MSG);
        exit (FAILURE);
    }

    std::string file_path_procs = file_path + PATH_PROCS;
    char* file_path_con2 = const_cast<char *>(file_path_procs.c_str());
    if (remove(file_path_con2) != 0) {
        fprintf (stderr, REMOVE_ERROR_MSG);
        exit (FAILURE);
    }

    // delete directories using rmdir()
    std::string file_path_pids = file_path + PATH_PIDS;
    char* file_path_con3 = const_cast<char *>(file_path_pids.c_str());
    if (rmdir(file_path_con3) != 0) {
        fprintf (stderr, REMOVE_ERROR_MSG);
        exit (FAILURE);
    }

    std::string file_path_cgroups = file_path + PATH_CGROUPS;
    char* file_path_con4 = const_cast<char *>(file_path_cgroups.c_str());
    if (rmdir(file_path_con4) != 0) {
        fprintf (stderr, REMOVE_ERROR_MSG);
        exit (FAILURE);
    }

    std::string file_path_fs = file_path + PATH_FS;
    char* file_path_con5 = const_cast<char *>(file_path_fs.c_str());
    if (rmdir(file_path_con5) != 0) {
        fprintf (stderr, REMOVE_ERROR_MSG);
        exit (FAILURE);
    }

    std::string file_path_umount = file_path + "/proc";
    char* file_path_con6 = const_cast<char *>(file_path_umount.c_str());
    if (umount(file_path_con6) != 0) {
        fprintf (stderr, UMOUNT_ERROR_MSG);
        exit (FAILURE);
    }
    return SUCCESS;
}
