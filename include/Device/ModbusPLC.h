#pragma once

#include "Device/IPLCInterface.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

#if __has_include(<modbus/modbus.h>)
#define HMVISION_MODBUS_AVAILABLE 1
#include <modbus/modbus.h>
#else
#define HMVISION_MODBUS_AVAILABLE 0
struct _modbus;
using modbus_t = _modbus;
#endif

#if __has_include(<snap7.h>)
#define HMVISION_S7_AVAILABLE 1
#include <snap7.h>
#else
#define HMVISION_S7_AVAILABLE 0
class TS7Client;
#endif

namespace HMVision
{
class ModbusPLC : public IPLCInterface
{
public:
    ModbusPLC();
    ~ModbusPLC() override;

    bool connect(const ConnectionParams& params) override;
    bool disconnect() override;
    bool readInput(int address, int& value) override;
    bool writeOutput(int address, int value) override;
    bool getConnectionStatus() override;
    void setHeartbeat(int intervalMs) override;
    bool readInputs(int startAddress, int count, std::vector<int>& values) override;
    bool writeOutputs(int startAddress, const std::vector<int>& values) override;

private:
    void heartbeatThread();
    bool reconnect();
    bool connectModbus();
    bool connectS7();
    bool disconnectInternal();

    ConnectionParams m_params;
    modbus_t* m_ctx = nullptr;
#if HMVISION_S7_AVAILABLE
    TS7Client* m_s7Client = nullptr;
#else
    void* m_s7Client = nullptr;
#endif
    std::string m_ip;
    int m_port = 502;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_running{false};
    std::thread m_heartbeatThread;
    std::atomic<int> m_heartbeatInterval{1000};
    std::mutex m_mutex;
    std::unordered_map<int, int> m_shadowRegisters;
};
} // namespace HMVision
