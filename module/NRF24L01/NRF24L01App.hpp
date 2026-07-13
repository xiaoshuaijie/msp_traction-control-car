#pragma once

#include <array>
#include <cstdint>

#include "app_framework.hpp"
#include "nrf24l01.h"

class NRF24L01App : public LibXR::Application
{
 public:
  struct Config
  {
    uint32_t poll_period_ms = 10;
  };

  struct State
  {
    uint8_t receive_status = 0;
    uint32_t receive_count = 0;
    uint32_t error_count = 0;
    std::array<uint8_t, NRF24L01_RX_PACKET_WIDTH> receive_data{};
  };

  /** Constructs an adapter with the default polling configuration. */
  explicit NRF24L01App(LibXR::ApplicationManager& app);

  /** Constructs and registers an adapter with the application manager. */
  NRF24L01App(LibXR::ApplicationManager& app, const Config& config);

  /** Initializes the driver once, then polls it at the configured period. */
  void OnMonitor() override;

  /** Returns the latest receive status, counters, and packet snapshot. */
  [[nodiscard]] const State& GetState() const { return state_; }

 private:
  Config config_;
  State state_;
  uint32_t last_poll_ms_ = 0;
  bool initialized_ = false;
};
