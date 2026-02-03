#ifndef MOCK_LITTLEFS_H
#define MOCK_LITTLEFS_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Mock File class for LittleFS
class File {
   public:
    File() : valid(false), position(0) {}
    File(const std::string& path, const std::vector<uint8_t>& data)
        : valid(true), path(path), data(data), position(0) {}

    operator bool() const { return valid; }

    size_t write(uint8_t byte) {
        if (!valid)
            return 0;
        if (position >= data.size()) {
            data.push_back(byte);
        } else {
            data[position] = byte;
        }
        position++;
        return 1;
    }

    size_t write(const uint8_t* buf, size_t size) {
        if (!valid)
            return 0;
        for (size_t i = 0; i < size; i++) {
            write(buf[i]);
        }
        return size;
    }

    int read() {
        if (!valid || position >= data.size()) {
            return -1;
        }
        return data[position++];
    }

    size_t read(uint8_t* buf, size_t size) {
        if (!valid)
            return 0;
        size_t bytesRead = 0;
        while (bytesRead < size && position < data.size()) {
            buf[bytesRead++] = data[position++];
        }
        return bytesRead;
    }

    int available() {
        if (!valid)
            return 0;
        return static_cast<int>(data.size() - position);
    }

    void close() { valid = false; }

    bool seek(size_t pos) {
        if (!valid)
            return false;
        position = pos;
        return true;
    }

    size_t size() const { return data.size(); }

    const std::string& name() const { return path; }

    const std::vector<uint8_t>& getData() const { return data; }

    void flush() {
        // No-op for mock
    }

   private:
    bool valid;
    std::string path;
    std::vector<uint8_t> data;
    size_t position;
};

// Mock LittleFS class
class LittleFSClass {
   public:
    LittleFSClass() : mounted(false), simulateMountFailure(false) {}

    bool begin(bool formatOnFail = false) {
        (void)formatOnFail;
        if (simulateMountFailure) {
            return false;
        }
        mounted = true;
        return true;
    }

    void end() {
        mounted = false;
        files.clear();
    }

    File open(const char* path, const char* mode = "r") {
        if (!mounted) {
            return File();
        }

        std::string pathStr(path);
        std::string modeStr(mode);

        if (modeStr == "r") {
            // Read mode
            if (files.find(pathStr) != files.end()) {
                return File(pathStr, files[pathStr]);
            }
            return File();  // File not found
        } else if (modeStr == "w") {
            // Write mode - create or truncate
            files[pathStr] = std::vector<uint8_t>();
            return File(pathStr, files[pathStr]);
        }

        return File();
    }

    bool exists(const char* path) {
        if (!mounted)
            return false;
        return files.find(std::string(path)) != files.end();
    }

    bool remove(const char* path) {
        if (!mounted)
            return false;
        std::string pathStr(path);
        if (files.find(pathStr) != files.end()) {
            files.erase(pathStr);
            return true;
        }
        return false;
    }

    bool rename(const char* pathFrom, const char* pathTo) {
        if (!mounted)
            return false;
        std::string fromStr(pathFrom);
        std::string toStr(pathTo);

        if (files.find(fromStr) != files.end()) {
            files[toStr] = files[fromStr];
            files.erase(fromStr);
            return true;
        }
        return false;
    }

    // Test helpers
    void setFileContent(const std::string& path, const std::vector<uint8_t>& content) {
        files[path] = content;
    }

    void setFileContent(const std::string& path, const std::string& content) {
        files[path] = std::vector<uint8_t>(content.begin(), content.end());
    }

    const std::vector<uint8_t>& getFileContent(const std::string& path) const {
        static std::vector<uint8_t> empty;
        auto it = files.find(path);
        if (it != files.end()) {
            return it->second;
        }
        return empty;
    }

    void clearFiles() { files.clear(); }

    void setSimulateMountFailure(bool fail) { simulateMountFailure = fail; }

    bool isMounted() const { return mounted; }

   private:
    bool mounted;
    bool simulateMountFailure;
    std::map<std::string, std::vector<uint8_t>> files;
};

// Global LittleFS instance
extern LittleFSClass LittleFS;

#endif  // MOCK_LITTLEFS_H
