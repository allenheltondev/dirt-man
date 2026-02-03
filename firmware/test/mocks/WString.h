#ifndef MOCK_WSTRING_H
#define MOCK_WSTRING_H

#include <cstring>
#include <string>

// Mock Arduino String class using std::string
class String {
   public:
    String() : str() {}
    String(const char* cstr) : str(cstr ? cstr : "") {}
    String(const std::string& s) : str(s) {}
    String(int value) : str(std::to_string(value)) {}
    String(unsigned int value) : str(std::to_string(value)) {}
    String(long value) : str(std::to_string(value)) {}
    String(unsigned long value) : str(std::to_string(value)) {}
    String(float value, int decimalPlaces = 2) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.*f", decimalPlaces, value);
        str = buffer;
    }
    String(double value, int decimalPlaces = 2) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.*f", decimalPlaces, value);
        str = buffer;
    }

    // Conversion operators
    operator const char*() const { return str.c_str(); }
    const char* c_str() const { return str.c_str(); }

    // Comparison operators
    bool operator==(const String& rhs) const { return str == rhs.str; }
    bool operator!=(const String& rhs) const { return str != rhs.str; }
    bool operator==(const char* cstr) const { return str == (cstr ? cstr : ""); }
    bool operator!=(const char* cstr) const { return str != (cstr ? cstr : ""); }

    // Concatenation operators
    String operator+(const String& rhs) const { return String(str + rhs.str); }
    String operator+(const char* cstr) const { return String(str + (cstr ? cstr : "")); }
    String& operator+=(const String& rhs) {
        str += rhs.str;
        return *this;
    }
    String& operator+=(const char* cstr) {
        str += (cstr ? cstr : "");
        return *this;
    }
    String& operator+=(char c) {
        str += c;
        return *this;
    }

    // Array access
    char operator[](unsigned int index) const { return (index < str.length()) ? str[index] : 0; }
    char& operator[](unsigned int index) { return str[index]; }

    // String methods
    unsigned int length() const { return static_cast<unsigned int>(str.length()); }
    void clear() { str.clear(); }
    bool isEmpty() const { return str.empty(); }

    String substring(unsigned int beginIndex) const {
        if (beginIndex >= str.length())
            return String();
        return String(str.substr(beginIndex));
    }

    String substring(unsigned int beginIndex, unsigned int endIndex) const {
        if (beginIndex >= str.length())
            return String();
        if (endIndex > str.length())
            endIndex = static_cast<unsigned int>(str.length());
        if (endIndex <= beginIndex)
            return String();
        return String(str.substr(beginIndex, endIndex - beginIndex));
    }

    int indexOf(char c, unsigned int fromIndex = 0) const {
        size_t pos = str.find(c, fromIndex);
        return (pos != std::string::npos) ? static_cast<int>(pos) : -1;
    }

    int indexOf(const String& s, unsigned int fromIndex = 0) const {
        size_t pos = str.find(s.str, fromIndex);
        return (pos != std::string::npos) ? static_cast<int>(pos) : -1;
    }

    bool startsWith(const String& prefix) const { return str.find(prefix.str) == 0; }

    bool endsWith(const String& suffix) const {
        if (suffix.length() > str.length())
            return false;
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix.str) == 0;
    }

    void toLowerCase() {
        for (char& c : str) {
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
        }
    }

    void toUpperCase() {
        for (char& c : str) {
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
        }
    }

    void trim() {
        // Trim leading whitespace
        size_t start = 0;
        while (start < str.length() && isspace(str[start])) {
            start++;
        }
        // Trim trailing whitespace
        size_t end = str.length();
        while (end > start && isspace(str[end - 1])) {
            end--;
        }
        str = str.substr(start, end - start);
    }

    int toInt() const { return std::stoi(str); }
    float toFloat() const { return std::stof(str); }
    double toDouble() const { return std::stod(str); }

    // Access to underlying std::string
    const std::string& stdStr() const { return str; }

   private:
    std::string str;
};

#endif  // MOCK_WSTRING_H
