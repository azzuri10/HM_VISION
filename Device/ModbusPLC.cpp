#include "ModbusPLC.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace HMVision
{
ModbusPLC::ModbusPLC() = default;

ModbusPLC::~ModbusPLC()
{
    disconnect();
}

bool ModbusPLC::connect(const ConnectionParams& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_params = params;
    m_ip = params.ip;
    m_port = params.port;

    disconnectInternal();

    bool ok = false;
    if (m_params.protocol == PLCProtocol::ModbusTcp)
    {
        ok = connectModbus();
    }
    else
    {
        ok = connectS7();
    }

    m_connected.store(ok);
    if (ok && !m_running.load())
    {
        m_running.store(true);
        m_heartbeatThread = std::thread(&ModbusPLC::heartbeatThread, this);
    }
    return ok;
}

bool ModbusPLC::disconnect()
{
    m_running.store(false);
    if (m_heartbeatThread.joinable())
    {
        m_heartbeatThread.join();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_connected.store(false);
    return disconnectInternal();
}

bool ModbusPLC::readInput(int address, int& value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected.load())
    {
        return false;
    }

    if (m_params.protocol == PLCProtocol::ModbusTcp)
    {
#if HMVISION_MODBUS_AVAILABLE
        uint16_t reg = 0;
        const int rc = modbus_read_registers(m_ctx, address, 1, &reg);
        if (rc == 1)
        {
            value = static_cast<int>(reg);
            m_shadowRegisters[address] = value;
            return true;
        }
        return false;
#else
        const auto it = m_shadowRegisters.find(address);
        if (it == m_shadowRegisters.end())
        {
            return false;
        }
        value = it->second;
        return true;
#endif
    }

#if HMVISION_S7_AVAILABLE
    if (m_s7Client == nullptr)
    {
        return false;
    }
    unsigned char buffer[2] = {0, 0};
    const int rc = m_s7Client->DBRead(1, address * 2, 2, buffer);
    if (rc == 0)
    {
        value = (static_cast<int>(buffer[0]) << 8) | static_cast<int>(buffer[1]);
        m_shadowRegisters[address] = value;
        return true;
    }
    return false;
#else
    const auto it = m_shadowRegisters.find(address);
    if (it == m_shadowRegisters.end())
    {
        return false;
    }
    value = it->second;
    return true;
#endif
}

bool ModbusPLC::writeOutput(int address, int value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected.load())
    {
        return false;
    }

    if (m_params.protocol == PLCProtocol::ModbusTcp)
    {
#if HMVISION_MODBUS_AVAILABLE
        const int rc = modbus_write_register(m_ctx, address, static_cast<uint16_t>(value));
        if (rc == 1)
        {
            m_shadowRegisters[address] = value;
            return true;
        }
        return false;
#else
        m_shadowRegisters[address] = value;
        return true;
#endif
    }

#if HMVISION_S7_AVAILABLE
    if (m_s7Client == nullptr)
    {
        return false;
    }
    unsigned char buffer[2] = {
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>(value & 0xFF)};
    const int rc = m_s7Client->DBWrite(1, address * 2, 2, buffer);
    if (rc == 0)
    {
        m_shadowRegisters[address] = value;
        return true;
    }
    return false;
#else
    m_shadowRegisters[address] = value;
    return true;
#endif
}

bool ModbusPLC::getConnectionStatus()
{
    return m_connected.load();
}

void ModbusPLC::setHeartbeat(int intervalMs)
{
    m_heartbeatInterval.store(std::max(100, intervalMs));
}

bool ModbusPLC::readInputs(int startAddress, int count, std::vector<int>& values)
{
    values.clear();
    if (count <= 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected.load())
    {
        return false;
    }

    if (m_params.protocol == PLCProtocol::ModbusTcp)
    {
#if HMVISION_MODBUS_AVAILABLE
        std::vector<uint16_t> regs(static_cast<std::size_t>(count), 0);
        const int rc = modbus_read_registers(m_ctx, startAddress, count, regs.data());
        if (rc != count)
        {
            return false;
        }
        values.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const int v = static_cast<int>(regs[static_cast<std::size_t>(i)]);
            values.push_back(v);
            m_shadowRegisters[startAddress + i] = v;
        }
        return true;
#else
        values.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            values.push_back(m_shadowRegisters[startAddress + i]);
        }
        return true;
#endif
    }

#if HMVISION_S7_AVAILABLE
    if (m_s7Client == nullptr)
    {
        return false;
    }
    std::vector<unsigned char> buffer(static_cast<std::size_t>(count * 2), 0);
    const int rc = m_s7Client->DBRead(1, startAddress * 2, count * 2, buffer.data());
    if (rc != 0)
    {
        return false;
    }
    values.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        const int high = buffer[static_cast<std::size_t>(2 * i)];
        const int low = buffer[static_cast<std::size_t>(2 * i + 1)];
        const int v = (high << 8) | low;
        values.push_back(v);
        m_shadowRegisters[startAddress + i] = v;
    }
    return true;
#else
    values.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        values.push_back(m_shadowRegisters[startAddress + i]);
    }
    return true;
#endif
}

bool ModbusPLC::writeOutputs(int startAddress, const std::vector<int>& values)
{
    if (values.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected.load())
    {
        return false;
    }

    if (m_params.protocol == PLCProtocol::ModbusTcp)
    {
#if HMVISION_MODBUS_AVAILABLE
        std::vector<uint16_t> regs(values.size(), 0);
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            regs[i] = static_cast<uint16_t>(values[i]);
        }
        const int rc = modbus_write_registers(
            m_ctx, startAddress, static_cast<int>(values.size()), regs.data());
        if (rc != static_cast<int>(values.size()))
        {
            return false;
        }
#endif
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            m_shadowRegisters[startAddress + static_cast<int>(i)] = values[i];
        }
        return true;
    }

#if HMVISION_S7_AVAILABLE
    if (m_s7Client == nullptr)
    {
        return false;
    }
    std::vector<unsigned char> buffer(values.size() * 2, 0);
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        buffer[2 * i] = static_cast<unsigned char>((values[i] >> 8) & 0xFF);
        buffer[2 * i + 1] = static_cast<unsigned char>(values[i] & 0xFF);
    }
    const int rc = m_s7Client->DBWrite(
        1, startAddress * 2, static_cast<int>(buffer.size()), buffer.data());
    if (rc != 0)
    {
        return false;
    }
#endif
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        m_shadowRegisters[startAddress + static_cast<int>(i)] = values[i];
    }
    return true;
}

void ModbusPLC::heartbeatThread()
{
    while (m_running.load())
    {
        bool alive = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_params.protocol == PLCProtocol::ModbusTcp)
            {
#if HMVISION_MODBUS_AVAILABLE
                if (m_ctx != nullptr)
                {
                    uint16_t reg = 0;
                    alive = (modbus_read_registers(m_ctx, 0, 1, &reg) == 1);
                }
#else
                alive = true;
#endif
            }
            else
            {
#if HMVISION_S7_AVAILABLE
                if (m_s7Client != nullptr)
                {
                    alive = m_s7Client->Connected();
                }
#else
                alive = true;
#endif
            }
        }

        m_connected.store(alive);
        if (!alive)
        {
            reconnect();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(m_heartbeatInterval.load()));
    }
}

bool ModbusPLC::reconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    disconnectInternal();
    bool ok = false;
    if (m_params.protocol == PLCProtocol::ModbusTcp)
    {
        ok = connectModbus();
    }
    else
    {
        ok = connectS7();
    }
    m_connected.store(ok);
    return ok;
}

bool ModbusPLC::connectModbus()
{
#if HMVISION_MODBUS_AVAILABLE
    m_ctx = modbus_new_tcp(m_ip.c_str(), m_port);
    if (m_ctx == nullptr)
    {
        return false;
    }
    modbus_set_response_timeout(
        m_ctx, static_cast<uint32_t>(m_params.timeoutMs / 1000), static_cast<uint32_t>((m_params.timeoutMs % 1000) * 1000));
    return modbus_connect(m_ctx) == 0;
#else
    return true;
#endif
}

bool ModbusPLC::connectS7()
{
#if HMVISION_S7_AVAILABLE
    if (m_s7Client == nullptr)
    {
        m_s7Client = new TS7Client();
    }
    const int rc = m_s7Client->ConnectTo(m_ip.c_str(), m_params.rack, m_params.slot);
    return rc == 0;
#else
    return true;
#endif
}

bool ModbusPLC::disconnectInternal()
{
    bool ok = true;

#if HMVISION_MODBUS_AVAILABLE
    if (m_ctx != nullptr)
    {
        modbus_close(m_ctx);
        modbus_free(m_ctx);
        m_ctx = nullptr;
    }
#else
    m_ctx = nullptr;
#endif

#if HMVISION_S7_AVAILABLE
    if (m_s7Client != nullptr)
    {
        m_s7Client->Disconnect();
        delete m_s7Client;
        m_s7Client = nullptr;
    }
#else
    m_s7Client = nullptr;
#endif

    return ok;
}
} // namespace HMVision
