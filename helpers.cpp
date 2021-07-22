#include "helpers.h"

uint16_t helpers::channels_to_bitmask(std::vector<uint8_t> chans) {
    uint16_t mask = 0;

    // check for and remove duplicates
    // std::unique only removes consecutive dups. sort first
    std::sort(chans.begin(), chans.end());
    chans.erase(std::unique(chans.begin(), chans.end()), chans.end());

    for (uint8_t i=0; i<chans.size(); ++i) {
        mask += (mask | (1 << (chans[i]-1))) & (1 << (chans[i]-1));
    }

    return mask;
}

std::vector<uint8_t> helpers::bitmask_to_channels(uint16_t mask) {
    std::vector<uint8_t> channels;
    uint8_t bit =0;
    for (uint8_t n = 0; n<=16; ++n) {
        bit = (mask >> (n-1)) & 1U;
        if (bit == 1) {
            channels.push_back(n);
        }
    }

    return channels;
}
