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
#include <iostream>

#include "util.h"
#include "cgroup.h"


const size_t kStackSize = 4 * 1024 * 1024;

// Initialization parameters to booting a container
struct CTParams {
  std::string root_fs_dir;  // The absolute path for rootfs of docker image
  std::string image_path;   // The absolute path for image.tar
  ResourceConfig res_config;// Resources configuration
  std::string ip_addr;      //ip addr for veth
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
  int syscall_ret = umount((init_params->root_fs_dir + "/mountpoint").c_str());
  ASSERT_CHECK(syscall_ret, 0, "Unmount failed");
  syscall_ret = rmdir(init_params->root_fs_dir.c_str());
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

  return 0;
}


void parse_parameters(CTParams* params, int argc, char* argv[]) {

  ASSERT_CHECK((argc > 1), true, "Require at least one argument");
  params->image_path = std::string(argv[1]);
  params->root_fs_dir = std::string(argv[2]);
  if (argc > 3) { // resource limitation parameters
    params->res_config.mem_limit = std::string(argv[3]);
    params->res_config.cpu_period = std::string(argv[4]);
    params->res_config.cpu_quota = std::string(argv[5]);
  }
}

void initialize_veth(std::string ip, int pid){
  char cmd[512];

  std::sprintf(cmd,"sudo touch /var/run/netns/ns%d", pid);
  ASSERT_CHECK(system(cmd), 0, "create netns failed");

  std::sprintf(cmd,"mount --bind /proc/%d/ns/net /var/run/netns/ns%d", pid, pid);
  ASSERT_CHECK(system(cmd), 0, "mount netns failed");

  std::sprintf(cmd,"ip link add name veth%dh type veth peer name veth%dg", pid, pid);
  ASSERT_CHECK(system(cmd), 0, "create veth failed");

  std::sprintf(cmd,"ip link set veth%dg netns ns%d", pid, pid);
  ASSERT_CHECK(system(cmd), 0, "set veth namespace failed");

  std::sprintf(cmd,"ip netns exec ns%d ip link set dev veth%dg name eth0", pid, pid);
  ASSERT_CHECK(system(cmd), 0, "set veth namespace failed");

  std::sprintf(cmd,"ip netns exec ns%d ip address add %s dev eth0", pid, ip.c_str());
  ASSERT_CHECK(system(cmd), 0, "set ip addr failed");

  std::sprintf(cmd,"ip netns exec ns%d ip link set dev eth0 up", pid);
  ASSERT_CHECK(system(cmd), 0, "set veth up failed");
  return;
}

int main(int argc, char* argv[]) {
  CTParams init_params;
  // parse parameters
  int opt = 0;
  while((opt = getopt(argc,argv,"hi:r:m:c:q:ka:")) !=-1){
    switch(opt){
      case 'h':
        printf(
        "usage:\n"
        "\t  -h                  : print usage \n"
        "\t  -i <path>           : select image path, should be absolute \n"
        "\t  -r <root_path>      : select root path\n"
        "\t  -m <memory volume>  : select mem limit, e.g 500m,200k \n"
        "\t  -k                  : when memory allocation exceeds control, whether keep it or not \n"
        "\t  -c <cpu_time_slice> : select the size of cpu time slice  in us, it should be >=1000 \n"
        "\t  -q <cpu quota >     : select the size of cpu quota in us, it should be >=1000 \n"
        "\t  -a <ip address>     : ip address for veth \n"
        );
        return 0;
      case 'i':
        init_params.image_path = std::string(optarg);
        break;
      case 'r':
        init_params.root_fs_dir = std::string(optarg);
        std::cout<<init_params.root_fs_dir<<std::endl;
        break;
      case 'm':
        init_params.res_config.mem_limit = std::string(optarg);
        break;
      case 'k':
        init_params.res_config.mem_keep  = true;
        break;
      case 'c':
        init_params.res_config.cpu_period = std::string(optarg);
        break;
      case 'q':
        init_params.res_config.cpu_quota = std::string(optarg);
        break;
      case 'a':
        init_params.ip_addr = std::string(optarg);
        break;
      case '?':
        printf("error optopt:%c\n",optopt);
        printf("error opterr:%c\n",opterr);
        exit(EXIT_FAILURE);
    }


  }

  //parse_parameters(&init_params, argc, argv);

  // spawn another process to run the container 
  auto stack = malloc(kStackSize);
  auto clone_flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET;
  auto flags = clone_flags | SIGCHLD;
  auto pid = clone(container_process, (char*)stack + kStackSize, flags, &init_params);

  if (pid < 0) {
    std::printf("Failed to create child process\n");
    exit(1);
  }
  if (pid > 0) {
    printf("Child pid=%d\n", pid);
    int wait_stat;

    // Initialize cgroup:
    CGroupManager cgroup("mydocker-cgroup", init_params.res_config);
    cgroup.Set();
    cgroup.Apply(pid);

    // Initialize veth
    initialize_veth(init_params.ip_addr, pid);
    
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

    // Destroy env after shell exits
    // DestroyContainerEnv(&init_params);
    // TODO: Destroy cgroup
    // TODO: Destory netns
  } else {  // Current process is child
    printf("I am child\n");
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}
