#pragma once
#include <cstdint>
#include<string>
#include <unordered_set>

struct ResourceConfig {
  std::string mem_limit; // memory limit expressed as string, e.g. "100m"
};

// Interface for subsystem resources controlling
class Subsystem {
public:
  virtual const std::string Name() const = 0;
  virtual void Set(const std::string& path, const ResourceConfig* config) = 0;
  virtual void Apply(const std::string& path, int pid) = 0;
  virtual ~Subsystem() = default;
};

class MemorySubsystem : public Subsystem {
public:
  ~MemorySubsystem() = default;
  MemorySubsystem() = default;
  const std::string Name() const override { 
    return std::string("MemorySubsystem"); 
  }

  void Set(const std::string& path, const ResourceConfig* config) override;

  void Apply(const std::string& path, int pid) override;
};

class CGroupManager {
public:
  CGroupManager(const std::string& path, const ResourceConfig& res_config)
    :path_(path), res_config_(res_config) {
    subsys_.insert(new MemorySubsystem());
  }
  ~CGroupManager() {
    for (auto p : subsys_) {
      delete p;
    }
  }

  void Apply(int pid) {
    for (auto p : subsys_) {
      p->Apply(path_, pid);
    }
  }

  void Set() {
    for (auto p : subsys_) {
      p->Set(path_, &res_config_);
    }
  }
private:
  std::string path_; // The name for this cgroup
  ResourceConfig res_config_;
  std::unordered_set<Subsystem*> subsys_;
};
