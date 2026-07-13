#include "NRF24L01App.hpp"

#include <algorithm>

#include "thread.hpp"

NRF24L01App::NRF24L01App(LibXR::ApplicationManager& app)
    : NRF24L01App(app, Config{})
{
}

NRF24L01App::NRF24L01App(LibXR::ApplicationManager& app,
                         const Config& config)
    : config_(config)
{
  app.Register(*this);
}

void NRF24L01App::OnMonitor()
{
  const uint32_t now = LibXR::Thread::GetTime();
  if (!initialized_)
  {
    NRF24L01_Init();
    initialized_ = true;
    last_poll_ms_ = now;
    return;
  }

  if (now - last_poll_ms_ < config_.poll_period_ms)
  {
    return;
  }
  last_poll_ms_ = now;

  const uint8_t receive_status = NRF24L01_Receive();
  state_.receive_status = receive_status;
  if (receive_status == 1U)
  {
    std::copy_n(NRF24L01_RxPacket, state_.receive_data.size(),
                state_.receive_data.begin());
    ++state_.receive_count;
  }
  else if (receive_status == 2U || receive_status == 3U)
  {
    ++state_.error_count;
  }
}
