#include "cgroup.h"
#include "util.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void MemorySubsystem::Set(const std::string& path, const ResourceConfig* res) {
  auto cgroup_path = "/sys/fs/cgroup/memory/" + path;

  struct stat dir_info;
  if ((stat(cgroup_path.c_str(), &dir_info)) != 0) {
    int syscall_ret = mkdir(cgroup_path.c_str(), 0777);
    ASSERT_CHECK(syscall_ret, 0, "Create directory failed");
  }

  if (res->mem_limit != "") {
    char cmd[512];
    sprintf(cmd, "echo \"%s\" > %s/memory.limit_in_bytes", res->mem_limit.c_str(), cgroup_path.c_str());
    int syscall_ret = system(cmd);
    ASSERT_CHECK(syscall_ret, 0, "Write memory limit in bytes failed");
  }
  if (res->mem_keep ==true){
    char cmd[512];
    sprintf(cmd, "echo 1 >> %s/memory.oom_control",cgroup_path.c_str());
    int syscall_ret = system(cmd);
    ASSERT_CHECK(syscall_ret, 0, "Write memory oom control in bytes failed");
  }
}

void MemorySubsystem::Apply(const std::string& path, int pid) {
  auto cgroup_path = "/sys/fs/cgroup/memory/" + path;
  struct stat dir_info;
  int syscall_ret = stat(cgroup_path.c_str(), &dir_info);
  // The cgroup directory must exists before we write tasks
  ASSERT_CHECK(syscall_ret, 0, "Cgroup directory not exist");

  char cmd[512];
  sprintf(cmd, "echo %s > %s/tasks", std::to_string(pid).c_str(), cgroup_path.c_str());
  syscall_ret = system(cmd);
  ASSERT_CHECK(syscall_ret, 0, "Write pid into tasks file failed");
}

void CPUSubsystem::Set(const std::string& path, const ResourceConfig* res) {
  auto cgroup_path = "/sys/fs/cgroup/cpu/" + path;

  struct stat dir_info;
  if ((stat(cgroup_path.c_str(), &dir_info)) != 0) {
    int syscall_ret = mkdir(cgroup_path.c_str(), 0777);
    ASSERT_CHECK(syscall_ret, 0, "Create directory failed");
  }

  if (res->cpu_period != "") {
    char cmd[512];
    sprintf(cmd, "echo %s > %s/cpu.cfs_period_us", res->cpu_period.c_str(), cgroup_path.c_str());
    int syscall_ret = system(cmd);
    ASSERT_CHECK(syscall_ret, 0, "Write CPU period failed");
  }

  if (res->cpu_quota != "") {
    
    char cmd[512];
    sprintf(cmd, "echo %s > %s/cpu.cfs_quota_us", res->cpu_quota.c_str(), cgroup_path.c_str());
    int syscall_ret = system(cmd);
    ASSERT_CHECK(syscall_ret, 0, "Write CPU quota failed");
    
  }
}

void CPUSubsystem::Apply(const std::string& path, int pid) {
  auto cgroup_path = "/sys/fs/cgroup/cpu/" + path;
  struct stat dir_info;
  int syscall_ret = stat(cgroup_path.c_str(), &dir_info);
  // The cgroup directory must exists before we write tasks
  ASSERT_CHECK(syscall_ret, 0, "Cgroup directory not exist");

  char cmd[512];
  sprintf(cmd, "echo %s > %s/tasks", std::to_string(pid).c_str(), cgroup_path.c_str());
  syscall_ret = system(cmd);
  ASSERT_CHECK(syscall_ret, 0, "Write pid into tasks file failed");
}
