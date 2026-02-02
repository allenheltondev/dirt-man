#include "SensorManager.h"

#include "PinConfig.h"

#ifdef UNIT_TEST
#include "MockSensor.h"
#endif

// Sensor status bit positions
#define SENSOR_BME280_BIT 0
#define SENSOR_DS18B20_BIT 1
#define SENSOR_SOIL_BIT 2

// Default calibration values (uncalibrated state)
#define DEFAULT_SOIL_DRY_ADC 3000
#define DEFAULT_SOIL_WET_ADC 1500

// Physical range constants for validation
#define BME280_TEMP_MIN -40.0f
#define BME280_TEMP_MAX 85.0f
#define BME280_HUMIDITY_MIN 0.0f
#define BME280_HUMIDITY_MAX 100.0f
#define BME280_PRESSURE_MIN 300.0f
#define BME280_PRESSURE_MAX 1100.0f
#define DS18B20_TEMP_MIN -55.0f
#define DS18B20_TEMP_MAX 125.0f
#define DEVICE_DISCONNECTED_C -127.0f

SensorManager::SensorManager()
    : oneWire(nullptr),
      ds18b20(nullptr),
      soilDryAdc(DEFAULT_SOIL_DRY_ADC),
      soilWetAdc(DEFAULT_SOIL_WET_ADC),
      sensorStatus(0) {}

SensorManager::~SensorManager() {
    if (ds18b20) {
        delete ds18b20;
        ds18b20 = nullptr;
    }
    if (oneWire) {
        delete oneWire;
        oneWire = nullptr;
    }
}

void SensorManager::initialize() {
    sensorStatus = 0;

#ifdef UNIT_TEST
    // Mock mode - all sensors available
    sensorStatus = (1 << SENSOR_BME280_BIT) | (1 << SENSOR_DS18B20_BIT) | (1 << SENSOR_SOIL_BIT);
    return;
#else
    // Real hardware mode - initialize sensors with retry logic

    // Initialize BME280 via I2C (3 retry attempts)
    bool bme280Success = false;
    for (int attempt = 0; attempt < 3 && !bme280Success; attempt++) {
        if (bme280.begin(0x76)) {  // Try default address 0x76
            bme280Success = true;
            sensorStatus |= (1 << SENSOR_BME280_BIT);
        } else if (bme280.begin(0x77)) {  // Try alternate address 0x77
            bme280Success = true;
            sensorStatus |= (1 << SENSOR_BME280_BIT);
        } else {
            delay(1000);  // Wait 1 second before retry
        }
    }

    // Initialize DS18B20 via OneWire (3 retry attempts)
    bool ds18b20Success = false;
    for (int attempt = 0; attempt < 3 && !ds18b20Success; attempt++) {
        if (!oneWire) {
            oneWire = new OneWire(ONEWIRE_PIN);
        }
        if (!ds18b20) {
            ds18b20 = new DallasTemperature(oneWire);
        }

        ds18b20->begin();
        delay(100);  // Give sensor time to initialize

        // Test if sensor responds
        ds18b20->requestTemperatures();
        delay(750);  // Wait for conversion (750ms for 12-bit resolution)
        float testTemp = ds18b20->getTempCByIndex(0);

        if (testTemp != DEVICE_DISCONNECTED_C) {
            ds18b20Success = true;
            sensorStatus |= (1 << SENSOR_DS18B20_BIT);
        } else {
            delay(1000);  // Wait 1 second before retry
        }
    }

    // Configure ADC for soil moisture (no retry needed for ADC configuration)
    analogSetAttenuation(ADC_ATTENUATION);
    analogSetWidth(ADC_WIDTH);

    // Test ADC by reading once
    uint16_t testAdc = analogRead(SOIL_MOISTURE_PIN);
    if (testAdc >= 0 && testAdc <= 4095) {
        sensorStatus |= (1 << SENSOR_SOIL_BIT);
    }
#endif
}

SensorReadings SensorManager::readSensors() {
    SensorReadings readings = {};
    readings.monotonicMs = millis();
    readings.sensorStatus = 0;

#ifdef UNIT_TEST
    // Mock mode - return realistic test data
    readings.bme280Temp = MockSensor::BME280_TEMP_C;
    readings.humidity = MockSensor::BME280_HUMIDITY_PCT;
    readings.pressure = MockSensor::BME280_PRESSURE_HPA;
    readings.ds18b20Temp = MockSensor::DS18B20_TEMP_C;
    readings.soilMoistureRaw = MockSensor::SOIL_MOISTURE_RAW;
    readings.soilMoisture = convertSoilMoistureToPercent(readings.soilMoistureRaw);
    readings.sensorStatus = sensorStatus;  // Use initialized sensor status
    return readings;
#else
    // Real hardware mode - read from actual sensors

    // Read BME280 (temperature, humidity, pressure)
    if (sensorStatus & (1 << SENSOR_BME280_BIT)) {
        readings.bme280Temp = readBME280Temperature();
        readings.humidity = readBME280Humidity();
        readings.pressure = readBME280Pressure();

        // Validate readings
        if (validateReading(SensorType::BME280_TEMP, readings.bme280Temp) &&
            validateReading(SensorType::HUMIDITY, readings.humidity) &&
            validateReading(SensorType::PRESSURE, readings.pressure)) {
            readings.sensorStatus |= (1 << SENSOR_BME280_BIT);
        }
    }

    // Read DS18B20 (temperature)
    if (sensorStatus & (1 << SENSOR_DS18B20_BIT)) {
        readings.ds18b20Temp = readDS18B20Temperature();

        // Validate reading
        if (validateReading(SensorType::DS18B20_TEMP, readings.ds18b20Temp)) {
            readings.sensorStatus |= (1 << SENSOR_DS18B20_BIT);
        }
    }

    // Read soil moisture with ADC averaging
    if (sensorStatus & (1 << SENSOR_SOIL_BIT)) {
        readings.soilMoistureRaw = readSoilMoistureRaw();
        readings.soilMoisture = convertSoilMoistureToPercent(readings.soilMoistureRaw);

        // Validate reading (ADC should be in valid range)
        if (readings.soilMoistureRaw >= 0 && readings.soilMoistureRaw <= 4095) {
            readings.sensorStatus |= (1 << SENSOR_SOIL_BIT);
        }
    }

    return readings;
#endif
}

bool SensorManager::isSensorAvailable(SensorType type) {
    // Check sensor status bitmask for the given sensor type
    switch (type) {
        case SensorType::BME280_TEMP:
        case SensorType::HUMIDITY:
        case SensorType::PRESSURE:
            return (sensorStatus & (1 << SENSOR_BME280_BIT)) != 0;
        case SensorType::DS18B20_TEMP:
            return (sensorStatus & (1 << SENSOR_DS18B20_BIT)) != 0;
        case SensorType::SOIL_MOISTURE:
            return (sensorStatus & (1 << SENSOR_SOIL_BIT)) != 0;
        default:
            return false;
    }
}

void SensorManager::calibrateSoilMoisture(uint16_t dryAdc, uint16_t wetAdc) {
    // Validate ADC values are in range 0-4095
    if (dryAdc > 4095 || wetAdc > 4095) {
        // Invalid calibration values - keep existing values
        return;
    }

    // Validate that dry and wet values are different
    if (dryAdc == wetAdc) {
        // Cannot calibrate with identical values - keep existing values
        return;
    }

    // Store calibration values
    soilDryAdc = dryAdc;
    soilWetAdc = wetAdc;
}

// Private helper methods

float SensorManager::readBME280Temperature() {
#ifdef UNIT_TEST
    return MockSensor::BME280_TEMP_C;
#else
    return bme280.readTemperature();
#endif
}

float SensorManager::readBME280Humidity() {
#ifdef UNIT_TEST
    return MockSensor::BME280_HUMIDITY_PCT;
#else
    return bme280.readHumidity();
#endif
}

float SensorManager::readBME280Pressure() {
#ifdef UNIT_TEST
    return MockSensor::BME280_PRESSURE_HPA;
#else
    // BME280 returns pressure in Pa, convert to hPa
    return bme280.readPressure() / 100.0f;
#endif
}

float SensorManager::readDS18B20Temperature() {
#ifdef UNIT_TEST
    return MockSensor::DS18B20_TEMP_C;
#else
    if (!ds18b20) {
        return DEVICE_DISCONNECTED_C;
    }

    // Request temperature conversion
    ds18b20->requestTemperatures();

    // Wait for conversion (750ms for 12-bit resolution)
    delay(750);

    // Read temperature
    return ds18b20->getTempCByIndex(0);
#endif
}

float SensorManager::readSoilMoisture() {
    uint16_t rawAdc = readSoilMoistureRaw();
    return convertSoilMoistureToPercent(rawAdc);
}

uint16_t SensorManager::readSoilMoistureRaw() {
#ifdef UNIT_TEST
    return MockSensor::SOIL_MOISTURE_RAW;
#else
    // Multi-sample averaging to reduce noise (15 samples)
    const int NUM_SAMPLES = 15;
    uint32_t sum = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum += analogRead(SOIL_MOISTURE_PIN);
        delay(10);  // Small delay between samples
    }

    // Calculate average
    uint16_t average = sum / NUM_SAMPLES;
    return average;
#endif
}

float SensorManager::convertSoilMoistureToPercent(uint16_t rawAdc) {
    // Calibration formula: percentage = (rawAdc - soilDryAdc) / (soilWetAdc - soilDryAdc) * 100
    // Clamp result to 0-100 range

    if (soilWetAdc == soilDryAdc) {
        return 0.0f;  // Avoid division by zero
    }

    float percentage = (float)(rawAdc - soilDryAdc) / (float)(soilWetAdc - soilDryAdc) * 100.0f;

    // Clamp to 0-100 range
    if (percentage < 0.0f)
        percentage = 0.0f;
    if (percentage > 100.0f)
        percentage = 100.0f;

    return percentage;
}

bool SensorManager::validateReading(SensorType type, float value) {
    // Check if value is within physically possible range for sensor type
    switch (type) {
        case SensorType::BME280_TEMP:
            return (value >= BME280_TEMP_MIN && value <= BME280_TEMP_MAX);
        case SensorType::DS18B20_TEMP:
            return (value >= DS18B20_TEMP_MIN && value <= DS18B20_TEMP_MAX &&
                    value != DEVICE_DISCONNECTED_C);
        case SensorType::HUMIDITY:
            return (value >= BME280_HUMIDITY_MIN && value <= BME280_HUMIDITY_MAX);
        case SensorType::PRESSURE:
            return (value >= BME280_PRESSURE_MIN && value <= BME280_PRESSURE_MAX);
        case SensorType::SOIL_MOISTURE:
            return (value >= 0.0f && value <= 100.0f);
        default:
            return false;
    }
}
