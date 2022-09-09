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
#include "kali_structs.h"

std::string ip = "10.42.0.104";
uint16_t port = 37397;

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
