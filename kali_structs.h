#ifndef KALI_STRUCTS_H
#define KALI_STRUCTS_H

#include <cstdint>
#include <vector>
#include <string>

enum returncodes: int8_t {
    UNKNOWN_ERROR   = -3,
    NOT_FOUND       = -2,
    NOT_DONE        = -1,
    DONE            =  0
};

struct tagger_info_struct {
    int fpga_version;
    float resolution;
    int num_inputs;
};

struct jobstruct {
    uint64_t id = 0;                                // numeric ID of the job
    std::vector<unsigned short int> patterns;       // list of bitmasks of coincidence patterns client is interested in
    std::vector<unsigned long long int> events;     // vector holding total events in of corresponding patterns
    unsigned short int window;                      // clients coincidence window in ns
    uint64_t duration;                              // Duration during which thise events occurred (in internal ticks)
    double duration_s;                              // Duration during which thise events occurred (in seconds)
    unsigned long long int start_tag;               // first tag in job
    unsigned long long int stop_tag;                // last tag in job
    bool finished = false;                          // true if job completed
};

struct tagfileinfo {
    uint64_t id = 0;                                // numeric ID of the job
    std::string filename;                           // filename to save tags to
    std::vector<uint8_t> channels;                  // channels to save
    uint64_t duration;                              // Duration during which thise events occurred (in internal ticks)
    unsigned long long int start_tag;               // first tag in job
    unsigned long long int stop_tag;                // last tag in job
    bool converting = false;                        // true if tag file conversion in progress
    bool converted = false;                         // true if tag file conversion done
    bool finished = false;                          // true if job completed
};

#endif // KALI_STRUCTS_H
