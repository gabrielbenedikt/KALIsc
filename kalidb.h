#ifndef KALIDB_H
#define KALIDB_H

#include <sqlite3.h>
#include <stdint.h>     //uintX_t
#include <vector>       //std::vector
#include <string>       //std::string
#include <iostream>     //cout
#include <fstream>
#include <sstream>

#include "kali_structs.h"

// capnproto
#include "cp_tags.capnp.h"
#include "ttdata.capnp.h"
#include <kj/debug.h>
#include <kj/async-io.h>
#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>

// stream to base64 encoding
#include <boost/iostreams/device/array.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
typedef boost::archive::iterators::transform_width< boost::archive::iterators::binary_from_base64<std::string::const_iterator >, 8, 6 > it_binary_t;
typedef boost::archive::iterators::base64_from_binary<boost::archive::iterators::transform_width<std::string::const_iterator ,6,8> > it_base64_t;

//compression
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filter/zstd.hpp>
#include <zstd.h>

class kalidb
{
public:
    kalidb();
    int open_db();
    void job_to_db(jobstruct &job);                     // save a job to the database
    int job_from_db(uint64_t id, jobstruct &out_job);   // read a job from the database
    uint64_t get_max_db_id();                            // get a new measrement id
    void set_tagger_info(tagger_info_struct &ti);

private:
    // database
    sqlite3* DB;
    tagger_info_struct tagger_info;
};

#endif // KALIDB_H
