#ifndef KALICNC_CLIENT
#define KALICNC_CLIENT

#include <bitset>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <csignal>
#include <thread>
#include <vector>


#include "cp_tags.capnp.h"
#include <capnp/ez-rpc.h>
#include <kj/debug.h>
#include <math.h>
#include <iostream>

// compression over network socket
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/zstd.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/asio.hpp>
#include <iostream>

#include <ncurses.h>

#include "helpers.h"


std::string ip = "10.42.0.13";
uint16_t port = 37397;

enum returncodes: int8_t {
    UNKNOWN_ERROR   = -3,
    NOT_FOUND       = -2,
    NOT_DONE        = -1,
    DONE            =  0
};

struct jobstruct {
    uint64_t id = 0;                                // numeric ID of the job
    std::vector<unsigned short int> patterns;       // list of bitmasks of coincidence patterns client is interested in
    std::vector<unsigned long long int> events;     // vector holding total events in of corresponding patterns
    unsigned short int window;                      // clients coincidence window
    uint64_t duration;                              // Duration during which thise events occurred (in internal ticks)
    double duration_s;                              // Duration during which thise events occurred (in seconds)
    unsigned long long int start_tag;               // first tag in job
    unsigned long long int stop_tag;                // last tag in job
    bool finished = false;                          // true if job completed
};

uint64_t submit_job(double duration);
int get_result(uint64_t id, jobstruct &job);

int8_t query_job_done(uint64_t id);
void print_job(jobstruct &job);

//ui
void init_ui();
void update_screen(jobstruct &job);

//signal handling
void shutdown(int signum);

#endif //KALICNC_H
