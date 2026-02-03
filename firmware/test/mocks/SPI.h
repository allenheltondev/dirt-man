#ifndef MOCK_SPI_H
#define MOCK_SPI_H

#include <cstdint>
#include <vector>

// Mock SPI class for native testing
class SPIClass {
   public:
    SPIClass() : initialized(false), clockSpeed(0), bitOrder(0), dataMode(0) {}

    void begin() { initialized = true; }
    void end() { initialized = false; }

    void beginTransaction(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
        this->clockSpeed = clock;
        this->bitOrder = bitOrder;
        this->dataMode = dataMode;
    }

    void endTransaction() {
        clockSpeed = 0;
        bitOrder = 0;
        dataMode = 0;
    }

    uint8_t transfer(uint8_t data) {
        // Store sent data and return mock response
        sentData.push_back(data);
        if (!responseData.empty()) {
            uint8_t response = responseData.front();
            responseData.erase(responseData.begin());
            return response;
        }
        return 0xFF;  // Default response
    }

    // Test helpers
    void setResponseData(const std::vector<uint8_t>& data) { responseData = data; }

    const std::vector<uint8_t>& getSentData() const { return sentData; }

    void clearSentData() { sentData.clear(); }

    bool isInitialized() const { return initialized; }

   private:
    bool initialized;
    uint32_t clockSpeed;
    uint8_t bitOrder;
    uint8_t dataMode;
    std::vector<uint8_t> sentData;
    std::vector<uint8_t> responseData;
};

// Global SPI instance
extern SPIClass SPI;

// SPI constants
#define SPI_MODE0 0x00
#define SPI_MODE1 0x01
#define SPI_MODE2 0x02
#define SPI_MODE3 0x03

#define MSBFIRST 1
#define LSBFIRST 0

#endif  // MOCK_SPI_H
