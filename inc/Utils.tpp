#include "colors.hpp"

template <typename T>
std::string toString(T value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}
