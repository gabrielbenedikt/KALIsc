#ifndef HELPERS_H
#define HELPERS_H

#include <vector>
#include <cstdint>

class helpers
{
public:
    static uint16_t channels_to_bitmask(std::vector<uint8_t> chans);

    static std::vector<uint8_t> bitmask_to_channels(uint16_t mask);
};

#endif // HELPERS_H
