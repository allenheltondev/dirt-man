#ifndef MOCK_WIRE_H
#define MOCK_WIRE_H

#include <cstdint>
#include <map>
#include <vector>

// Mock Wire (I2C) class for native testing
class TwoWire {
   public:
    TwoWire() : currentAddress(0), transmitting(false), simulateTimeout(false) {}

    void begin() { initialized = true; }
    void end() { initialized = false; }

    void beginTransmission(uint8_t address) {
        currentAddress = address;
        transmitting = true;
        txBuffer.clear();
    }

    uint8_t endTransmission(bool sendStop = true) {
        (void)sendStop;
        transmitting = false;

        // Simulate timeout if configured
        if (simulateTimeout) {
            return 4;  // I2C timeout error
        }

        // Check if device exists at address
        if (deviceResponses.find(currentAddress) == deviceResponses.end()) {
            return 2;  // NACK on address
        }

        return 0;  // Success
    }

    size_t write(uint8_t data) {
        if (transmitting) {
            txBuffer.push_back(data);
            return 1;
        }
        return 0;
    }

    size_t write(const uint8_t* data, size_t length) {
        if (transmitting) {
            for (size_t i = 0; i < length; i++) {
                txBuffer.push_back(data[i]);
            }
            return length;
        }
        return 0;
    }

    uint8_t requestFrom(uint8_t address, size_t length, bool sendStop = true) {
        (void)sendStop;
        currentAddress = address;
        rxBuffer.clear();

        // Simulate timeout if configured
        if (simulateTimeout) {
            return 0;
        }

        // Check if device exists and has response data
        if (deviceResponses.find(address) != deviceResponses.end()) {
            const auto& response = deviceResponses[address];
            size_t copyLen = (length < response.size()) ? length : response.size();
            rxBuffer.insert(rxBuffer.end(), response.begin(), response.begin() + copyLen);
            return static_cast<uint8_t>(copyLen);
        }

        return 0;  // No data available
    }

    int available() { return static_cast<int>(rxBuffer.size()); }

    int read() {
        if (rxBuffer.empty()) {
            return -1;
        }
        uint8_t data = rxBuffer.front();
        rxBuffer.erase(rxBuffer.begin());
        return data;
    }

    // Test helpers
    void setDeviceResponse(uint8_t address, const std::vector<uint8_t>& response) {
        deviceResponses[address] = response;
    }

    void removeDevice(uint8_t address) { deviceResponses.erase(address); }

    void clearDevices() { deviceResponses.clear(); }

    void setSimulateTimeout(bool timeout) { simulateTimeout = timeout; }

    const std::vector<uint8_t>& getTxBuffer() const { return txBuffer; }

    bool isInitialized() const { return initialized; }

   private:
    uint8_t currentAddress;
    bool transmitting;
    bool simulateTimeout;
    bool initialized = false;
    std::vector<uint8_t> txBuffer;
    std::vector<uint8_t> rxBuffer;
    std::map<uint8_t, std::vector<uint8_t>> deviceResponses;
};

// Global Wire instance
extern TwoWire Wire;

#endif  // MOCK_WIRE_H
