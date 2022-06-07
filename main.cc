#include <chrono>
#include <csignal>
#include <sched.h>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <thread>
#include <errno.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>

#define ASSERT_CHECK(val, expect, msg) \
{\
  if ((val) != (expect)) \
  {\
    std::cerr << msg << "\nexpect: " << expect << "\ngot: " << val;\
  }\
}

const size_t kStackSize = 4 * 1024 * 1024;

void container_init() {
  mount("proc", "/proc", "proc", MS_NOEXEC | MS_NOSUID | MS_NODEV, nullptr);
}

void pivotRoot(const std::string& root) {
  auto old_root = root + "/.pivot_root";

  auto syscall_ret = mkdir(old_root.c_str(), 0777);
  ASSERT_CHECK(syscall_ret, 0, "Create directory failed");

  // Old root is mounted to old_root directory
  syscall_ret = pivot_root(root.c_str(), old_root.c_str());
  ASSERT_CHECK(syscall_ret, 0, "pivot root call failed");

  // change current working directory to new root
  syscall_ret = chdir("/");
  ASSERT_CHECK(syscall_ret, 0, "chdir call failed");

  // Unmount old root
  syscall_ret = umount2("/.pivot_root", MNT_DETACH);
  ASSERT_CHECK(syscall_ret, 0, "unmount failed");

  // Remove old root
  syscall_ret = rmdir("/.pivot_root");
  ASSERT_CHECK(syscall_ret, 0, "remove directory failed");
}

int container_process(void* param) {
  (void)param;
  std::printf("Hello, World\n");
  std::printf("My pid is %d\n", getpid());

  // Container initialization
  container_init();

  // Open bash
  char* args[] = {"/bin/bash", nullptr};
  execv(args[0], args);

  return 0;
}

int main() {
  auto stack = malloc(kStackSize);
  auto clone_flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS;
  auto flags = clone_flags | SIGCHLD;
  auto pid = clone(container_process, (char*)stack + kStackSize, flags, nullptr);
  if (pid < 0) {
    std::printf("Failed to create child process");
    exit(1);
  }
  if (pid > 0) {
    printf("Child pid=%d\n", pid);
    int wait_stat;
    // auto ret = waitpid(pid, &wait_stat, WEXITED);
    
    while (true) {
      // auto ret = waitpid(pid, nullptr, WNOHANG);
      auto ret = wait(nullptr);
      if (ret < 0) {
        printf("error number is %d\n", errno);
      } else {
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } else {  // Current process is child
    printf("I am child\n");
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}
