#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>

#include "NRF24L01App.hpp"
#include "thread.hpp"

namespace
{

std::uint32_t g_init_call_count = 0;
std::uint32_t g_receive_call_count = 0;
std::uint8_t g_next_receive_status = 0;
int g_failure_count = 0;

/** Records a failed behavioral expectation. */
void Expect(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "FAIL: " << message << '\n';
    ++g_failure_count;
  }
}

/** Resets the fake driver and deterministic clock between scenarios. */
void ResetFixture()
{
  g_init_call_count = 0;
  g_receive_call_count = 0;
  g_next_receive_status = 0;
  std::fill_n(NRF24L01_RxPacket, NRF24L01_RX_PACKET_WIDTH, 0U);
  LibXR::Thread::SetTime(0);
}

/** Verifies construction registers the real adapter instance. */
void TestRegistersWithApplicationManager()
{
  ResetFixture();
  LibXR::ApplicationManager app;
  NRF24L01App adapter(app);

  Expect(app.registered_app == &adapter,
         "constructor must register the adapter instance");
  Expect(app.register_count == 1U,
         "constructor must register exactly once");
}

/** Verifies the first monitor call initializes without polling. */
void TestFirstMonitorOnlyInitializes()
{
  ResetFixture();
  LibXR::ApplicationManager app;
  NRF24L01App adapter(app);

  LibXR::Thread::SetTime(123U);
  adapter.OnMonitor();

  Expect(g_init_call_count == 1U,
         "first monitor call must initialize exactly once");
  Expect(g_receive_call_count == 0U,
         "first monitor call must not receive");
}

/** Verifies polling starts exactly at the configured 10 ms boundary. */
void TestPollPeriodBoundary()
{
  ResetFixture();
  LibXR::ApplicationManager app;
  NRF24L01App adapter(app);

  LibXR::Thread::SetTime(0U);
  adapter.OnMonitor();
  adapter.OnMonitor();
  Expect(g_receive_call_count == 0U, "0 ms must not poll");

  LibXR::Thread::SetTime(9U);
  adapter.OnMonitor();
  Expect(g_receive_call_count == 0U, "9 ms must not poll");

  LibXR::Thread::SetTime(10U);
  adapter.OnMonitor();
  Expect(g_receive_call_count == 1U, "10 ms must poll exactly once");
}

/** Verifies receive statuses update packet and error state correctly. */
void TestReceiveStatusAccounting()
{
  ResetFixture();
  LibXR::ApplicationManager app;
  NRF24L01App adapter(app);

  LibXR::Thread::SetTime(0U);
  adapter.OnMonitor();

  g_next_receive_status = 0U;
  LibXR::Thread::SetTime(10U);
  adapter.OnMonitor();
  Expect(adapter.GetState().receive_status == 0U,
         "status 0 must be recorded");
  Expect(adapter.GetState().receive_count == 0U,
         "status 0 must not increment receive count");
  Expect(adapter.GetState().error_count == 0U,
         "status 0 must not increment error count");

  for (std::uint32_t i = 0; i < NRF24L01_RX_PACKET_WIDTH; ++i)
  {
    NRF24L01_RxPacket[i] = static_cast<std::uint8_t>(i * 7U + 3U);
  }
  g_next_receive_status = 1U;
  LibXR::Thread::SetTime(20U);
  adapter.OnMonitor();
  Expect(adapter.GetState().receive_status == 1U,
         "status 1 must be recorded");
  Expect(adapter.GetState().receive_count == 1U,
         "status 1 must increment receive count");
  Expect(std::equal(adapter.GetState().receive_data.begin(),
                    adapter.GetState().receive_data.end(),
                    NRF24L01_RxPacket),
         "status 1 must copy the complete 32-byte packet");

  g_next_receive_status = 2U;
  LibXR::Thread::SetTime(30U);
  adapter.OnMonitor();
  Expect(adapter.GetState().error_count == 1U,
         "status 2 must increment error count");

  g_next_receive_status = 3U;
  LibXR::Thread::SetTime(40U);
  adapter.OnMonitor();
  Expect(adapter.GetState().error_count == 2U,
         "status 3 must increment error count");
  Expect(adapter.GetState().receive_count == 1U,
         "error statuses must not increment receive count");
  Expect(g_init_call_count == 1U,
         "error statuses must not reinitialize in the adapter");
}

/** Verifies unsigned elapsed-time arithmetic across uint32_t wraparound. */
void TestTimeWraparound()
{
  ResetFixture();
  LibXR::ApplicationManager app;
  NRF24L01App adapter(app);

  LibXR::Thread::SetTime(std::numeric_limits<std::uint32_t>::max() - 5U);
  adapter.OnMonitor();

  LibXR::Thread::SetTime(3U);
  adapter.OnMonitor();
  Expect(g_receive_call_count == 0U,
         "9 ms across uint32_t wrap must not poll");

  LibXR::Thread::SetTime(4U);
  adapter.OnMonitor();
  Expect(g_receive_call_count == 1U,
         "10 ms across uint32_t wrap must poll");
}

}  // namespace

extern "C"
{

std::uint8_t NRF24L01_TxAddress[NRF24L01_ADDRESS_WIDTH] = {};
std::uint8_t NRF24L01_TxPacket[NRF24L01_TX_PACKET_WIDTH] = {};
std::uint8_t NRF24L01_RxAddress[NRF24L01_ADDRESS_WIDTH] = {};
std::uint8_t NRF24L01_RxPacket[NRF24L01_RX_PACKET_WIDTH] = {};

std::uint8_t NRF24L01_ReadReg(std::uint8_t) { return 0U; }
void NRF24L01_ReadRegs(std::uint8_t, std::uint8_t*, std::uint8_t) {}
void NRF24L01_WriteReg(std::uint8_t, std::uint8_t) {}
void NRF24L01_WriteRegs(std::uint8_t, std::uint8_t*, std::uint8_t) {}
void NRF24L01_ReadRxPayload(std::uint8_t*, std::uint8_t) {}
void NRF24L01_WriteTxPayload(std::uint8_t*, std::uint8_t) {}
void NRF24L01_FlushTx() {}
void NRF24L01_FlushRx() {}
std::uint8_t NRF24L01_ReadStatus() { return 0U; }
void NRF24L01_PowerDown() {}
void NRF24L01_StandbyI() {}
void NRF24L01_Rx() {}
void NRF24L01_Tx() {}

void NRF24L01_Init() { ++g_init_call_count; }

std::uint8_t NRF24L01_Send() { return 0U; }

std::uint8_t NRF24L01_Receive()
{
  ++g_receive_call_count;
  return g_next_receive_status;
}

void NRF24L01_UpdateRxAddress() {}

}  // extern "C"

int main()
{
  TestRegistersWithApplicationManager();
  TestFirstMonitorOnlyInitializes();
  TestPollPeriodBoundary();
  TestReceiveStatusAccounting();
  TestTimeWraparound();

  if (g_failure_count != 0)
  {
    std::cerr << g_failure_count << " NRF24L01 adapter test(s) failed.\n";
    return 1;
  }

  std::cout << "NRF24L01 adapter behavior tests passed.\n";
  return 0;
}
