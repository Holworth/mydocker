#include <chrono>
#include <csignal>
#include <sched.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    std::printf("[ASSERTION FAILED] at %s, %d\n", __FILE__, __LINE__);\
    std::cerr << "[Error Msg]: " << msg << \
                 "\n[EXPECT]: " << expect << "\n[GOT]: " << val << \
                 "\n[Errno]: " << std::strerror(errno) << std::endl;\
    exit(1); \
  }\
}


const size_t kStackSize = 4 * 1024 * 1024;

// Initialization parameters to booting a container
struct CTParams {
  std::string root_fs_dir;  // The absolute path for rootfs of docker image
  std::string image_path;   // The absolute path for image.tar
};

void pivotRoot(const std::string& root);
void CreateReadonlyLayer(const std::string& image_path, const std::string& root_path);
void CreateWriteLayer(const std::string& root_path);
void CreateMountPoint(const std::string& root_path);
void CreateContainerEnv(const CTParams* init_params);
void DestroyContainerEnv(const CTParams* init_params);

void container_init(const CTParams* init_params) {
  CreateContainerEnv(init_params);
  pivotRoot(init_params->root_fs_dir + "/mountpoint");
  auto syscall_ret = mount("proc", "/proc", "proc", MS_NOEXEC | MS_NOSUID | MS_NODEV, nullptr);
  ASSERT_CHECK(syscall_ret, 0, "mount proc file system failed");
}

int pivot_root(const std::string& new_root, const std::string& old_root) {
  return syscall(SYS_pivot_root, new_root.c_str(), old_root.c_str());
}

void CreateContainerEnv(const CTParams* init_params) {
  int syscall_ret = mkdir(init_params->root_fs_dir.c_str(), 0777);
  ASSERT_CHECK(syscall_ret, 0, "Create container directory failed");
  CreateReadonlyLayer(init_params->image_path, init_params->root_fs_dir);
  CreateWriteLayer(init_params->root_fs_dir);
  CreateMountPoint(init_params->root_fs_dir);
}

void DestroyContainerEnv(const CTParams* init_params) {
  int syscall_ret = rmdir(init_params->root_fs_dir.c_str());
  ASSERT_CHECK(syscall_ret, 0, "Remove container directory failed");
}

// Root path is at the same level as image path by default
void CreateReadonlyLayer(const std::string& image_path, const std::string& root_path) {
  auto readonly_layer_path = root_path + "/readlayer";
  int syscall_ret = mkdir(readonly_layer_path.c_str(), 0777);
  ASSERT_CHECK(syscall_ret, 0, "Create readonly layer failed");

  char cmd[512];
  std::sprintf(cmd, "tar -xf %s -C %s", image_path.c_str(), readonly_layer_path.c_str());

  syscall_ret = system(cmd);
  ASSERT_CHECK(syscall_ret, 0, "Tar image failed");
}

void CreateWriteLayer(const std::string& root_path) {
  auto write_layer_path = root_path + "/writelayer";
  int syscall_ret = mkdir(write_layer_path.c_str(), 0777);
  ASSERT_CHECK(syscall_ret, 0, "Create Write Layer failed");
}

void CreateMountPoint(const std::string& root_path) {
  auto mount_path = root_path + "/mountpoint";
  auto readonly_layer_path = root_path + "/readlayer";
  auto write_layer_path = root_path + "/writelayer";
  int syscall_ret = mkdir(mount_path.c_str(), 0777);
  ASSERT_CHECK(syscall_ret, 0, "Create Mount Point failed");

  char cmd[512];
  std::sprintf(cmd, "mount -t aufs -o dirs=%s:%s none %s", write_layer_path.c_str(), 
               readonly_layer_path.c_str(), mount_path.c_str());
  syscall_ret = system(cmd);
  ASSERT_CHECK(syscall_ret, 0, "mount aufs failed");
}


void pivotRoot(const std::string& root) {
  auto old_root = root + "/.pivot_root";

  auto syscall_ret = mount(root.c_str(), root.c_str(), "bind", MS_BIND | MS_REC, nullptr);
  ASSERT_CHECK(syscall_ret, 0, "remount failed");

  syscall_ret = mkdir(old_root.c_str(), 0777);
  ASSERT_CHECK(syscall_ret, 0, "Create directory failed");

  // Old root is mounted to old_root directory
  syscall_ret = pivot_root(root, old_root);
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

  auto init_param = reinterpret_cast<const CTParams*>(param);

  // Container initialization
  container_init(init_param);

  // Run in shell
  char* args[] = {"/bin/sh", nullptr};
  auto syscall_ret = execv(args[0], args);
  ASSERT_CHECK(syscall_ret, 0, "Running shell failed");

  // Destroy env after shell exits
  DestroyContainerEnv(init_param);
  return 0;
}

void parse_parameters(CTParams* params, int argc, char* argv[]) {
  ASSERT_CHECK((argc > 1), true, "Require at least one argument");
  params->image_path = std::string(argv[1]);
  if (argc == 2) {
    // By default, fs dir is the same directory that cast off the last ".tar" characters
    params->root_fs_dir = std::string(argv[1]).substr(0, params->image_path.size() - 4);
  } else {
    params->root_fs_dir = std::string(argv[2]);
  }
}

int main(int argc, char* argv[]) {
  CTParams init_params;
  parse_parameters(&init_params, argc, argv);

  // spawn another process to run the container 
  auto stack = malloc(kStackSize);
  auto clone_flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS;
  auto flags = clone_flags | SIGCHLD;
  auto pid = clone(container_process, (char*)stack + kStackSize, flags, &init_params);

  if (pid < 0) {
    std::printf("Failed to create child process\n");
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
