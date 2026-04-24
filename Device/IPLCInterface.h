#pragma once

#include <string>
#include <vector>

namespace HMVision
{
enum class PLCProtocol
{
    ModbusTcp,
    S7
};

struct ConnectionParams
{
    PLCProtocol protocol = PLCProtocol::ModbusTcp;
    std::string ip = "127.0.0.1";
    int port = 502;
    int rack = 0;
    int slot = 1;
    int timeoutMs = 1000;
};

class IPLCInterface
{
public:
    virtual ~IPLCInterface() = default;

    virtual bool connect(const ConnectionParams& params) = 0;
    virtual bool disconnect() = 0;
    virtual bool readInput(int address, int& value) = 0;
    virtual bool writeOutput(int address, int value) = 0;
    virtual bool getConnectionStatus() = 0;
    virtual void setHeartbeat(int intervalMs) = 0;

    virtual bool readInputs(int startAddress, int count, std::vector<int>& values) = 0;
    virtual bool writeOutputs(int startAddress, const std::vector<int>& values) = 0;
};
} // namespace HMVision
