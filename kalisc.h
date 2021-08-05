#ifndef KALISC_H
#define KALISC_H

// time tagger
#include "include/CTimeTag.h"
#include "include/CLogic.h"

// database
#include "kalidb.h"

//structs
#include "kali_structs.h"

// usefull stuff
#include "helpers.h"

// capnproto
#include "cp_tags.capnp.h"
#include "ttdata.capnp.h"
#include <kj/debug.h>
#include <kj/async-io.h>
#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>

//c / c++
#include <bitset>
#include <chrono>       // performance measurement
#include <fcntl.h>      // file handling
#include <filesystem>
#include <fstream>      // file handling
#include <iterator>
#include <iostream>     
#include <math.h>     
#include <mutex>        // making tagbuffer threadsafe
#include <random>
#include <signal.h>
#include <sys/stat.h>   
#include <stdio.h>      
#include <stdlib.h>
#include <thread>       
#include <unistd.h>     
#include <vector>

//usb
#include <libusb-1.0/libusb.h>

//compression
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filter/zstd.hpp>
#include <zstd.h>

tagger_info_struct tagger_info;

// file extensions
std::string raw_chn_ext = "_.rawchn";   // for temporary file holding channels
std::string raw_tag_ext = "_.rawtag";   // for temporary file holding corresponding tags

uint16_t PORT = 37397;                  // port capnproto server listens to

TimeTag::CTimeTag tagger;
TimeTag::CLogic ttlogic(&tagger);

// tag buffer
std::vector<int> chan_buf;
std::vector<long long> tag_buf;
std::mutex tagbuf_mtx;

// database
kalidb DB;

//

std::vector<jobstruct> jobs;
std::mutex job_mtx;              // job vector

std::vector<tagfileinfo> tagfilequeue;
std::mutex tagqueue_mtx;


// menu / ui functions
void shutdown();
int menu();
int print_info();
void kbd_interrupt();
bool kbd_stop = false;

// catching ctrl+c
void signal_handler(int s);
void connect_signal_handler();

// timetagger functions
int tt_open();
int tt_close();
int tt_calibrate();
int tt_print_tags();
int tt_set_led_brightness(int percent);
int tt_get_fpga_version(int &out_version);
int tt_get_resolution(float &out_resolution);
int tt_get_num_inputs(int &out_inputs);
int tt_set_input_threshold(int chan, double voltage);
int tt_set_inversion_mask(int mask);
int tt_set_delay(int chan, int delay);
int tt_set_frequency_generator(int period, int high);
int tt_set_frequency_generator_hz(int hz);
int tt_use_ext_clock(bool use);
int tt_freeze_single_counter();
int tt_get_single_count(int chan);
int tt_set_filter_min_count(int min);
int tt_set_filter_max_time(int max);
int tt_set_filter_exception(int ex);
int tt_use_level_gate(bool use);
int tt_level_gate_active();
int tt_use_edge_gate(bool use);
int tt_set_edge_gate_width(int width);
int tt_save_raw_tags(std::string outfname);

// timetagger error functions
int tt_read_error_flags();
int tt_get_error_text(int flags);

// timetagger logic functions
int ttlogic_switch_logic_mode();
int ttlogic_set_window_width(int width);
int ttlogic_read_logic();
int ttlogic_calc_count(int pos, int neg);
int ttlogic_calc_count_pos(int pos);
int ttlogic_get_time_counter();
int ttlogic_set_output_event_count(int events);
int ttlogic_set_output_pattern(int output, int pos, int neg);
int ttlogic_set_output_width(int width);

///
// data processing
///
int convert_raw_tags(std::string infname, std::string outfname);
int test_tag_functions();
void fill_bufs();                       // fill chan_buf and tag_buf with random data
void process_tags();                    // get singles and events of patterns that are requested by connected clients
int tags_to_ns(std::vector<long long> &tags, std::vector<double> &out_ns);
int read_raw_tags(std::string fname, std::vector<int> &out_chan, std::vector<long long> &out_tags);
int read_text_tags(std::string fname, std::vector<int> &out_chan, std::vector<long long> &out_tags);
int average_rate(std::vector<double> &tags);
uint64_t calc_2f(std::vector<int> &chans, std::vector<long long int> &tags, std::vector<int> &pat_chans, uint16_t window);
void tagfile_converter();

//capnproto
int start_tag_server();         // start capnproto rpc server, aquire tags
void write_capnp_tags(std::string fname, std::vector<int> &channels, std::vector<long long> &tags);
void write_capnp_tags_compressed(std::string fname, std::vector<int> &channels, std::vector<long long> &tags);
void read_capnp_tags(std::string fname, std::vector<int> &out_channels, std::vector<long long> &out_tags);
void read_capnp_tags_compressed(std::string fname, std::vector<int> &out_channels, std::vector<long long> &out_tags);

// networking stuff
int send_tags_over_net();
int tagbuf_to_cap(TTdata::Builder &plb);

// job functions
uint64_t add_job(const Job::Reader &reader);    // add a job to the joblist
void del_job(uint64_t id);                      // remove a job from the joblist
void print_job(jobstruct &job);                 // print job info

//misc
uint64_t get_new_id();

#endif //KALISC_H
