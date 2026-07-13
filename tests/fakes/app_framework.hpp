#pragma once

#include <cstdint>

namespace LibXR
{

class Application
{
 public:
  virtual void OnMonitor() = 0;
  virtual ~Application() = default;
};

class ApplicationManager
{
 public:
  /** Records the application that production code registers. */
  void Register(Application& app)
  {
    registered_app = &app;
    ++register_count;
  }

  Application* registered_app = nullptr;
  std::uint32_t register_count = 0;
};

}  // namespace LibXR
