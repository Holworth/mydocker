#include <chrono>
#include <csignal>
#include <sched.h>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <errno.h>
#include <unistd.h>
#include <sys/mount.h>

const size_t kStackSize = 4 * 1024 * 1024;

void container_init() {
  mount("proc", "/proc", "proc", MS_NOEXEC | MS_NOSUID | MS_NODEV, nullptr);
}

int child_process(void* param) {
  (void)param;
  std::printf("Hello, World\n");
  std::printf("My pid is %d\n", getpid());

  // Do some necessary initiate work
  container_init();

  char* args[] = {"/bin/bash", nullptr};
  execv(args[0], args);

  return 0;
}

int main() {
  auto stack = malloc(kStackSize);
  auto clone_flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS;
  auto flags = clone_flags | SIGCHLD;
  auto pid = clone(child_process, (char*)stack + kStackSize, flags, nullptr);
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
