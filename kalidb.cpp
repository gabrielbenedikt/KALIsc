#include "kalidb.h"

kalidb::kalidb()
{
    tagger_info.fpga_version = 0;
    tagger_info.num_inputs = 0;
    tagger_info.resolution = 0;
}

int kalidb::open_db() {
    int ret = 0;
    std::string sql_cmd = "CREATE TABLE IF NOT EXISTS RESULTS("
                          "ID               UNSIGNED BIGINT PRIMARY KEY         NOT NULL, "
                          "DONE             INT                                 NOT NULL, "
                          "CAPRESMSG        BLOB                                );";
    ret=sqlite3_open("tagger_results.db", &DB);
    if (ret != 0) {
        std::cout << "Error opening database" << std::endl;
        return -1;
    } else {
        char* errMsg;
        ret = sqlite3_exec(DB, sql_cmd.c_str(), NULL, 0, &errMsg);
        if (ret != SQLITE_OK) {
            std::cout << "Error creating database table" << std::endl;
            sqlite3_free(errMsg);
        } else {
            return 0;
        }
        return -1;
    }
}

uint64_t kalidb::get_max_db_id() {
    int ret=-1;
    std::string sql_cmd = "SELECT MAX(ID) FROM RESULTS";

    sqlite3_stmt *stmt;
    ret = sqlite3_prepare_v2(DB, sql_cmd.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        std::string errMsg(sqlite3_errmsg(DB));
        std::cout << errMsg << std::endl;
        return -1;
    }
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW && ret != SQLITE_DONE) {
        std::string errMsg(sqlite3_errmsg(DB));
        sqlite3_finalize(stmt);
        std::cout << errMsg << std::endl;
    }
    if (ret == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            std::cout << std::string("not found") << std::endl;
            return -1;
    }
    uint64_t id_max = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return id_max;
}

void kalidb::job_to_db(jobstruct &job) {
    //serialize job

    ::capnp::MallocMessageBuilder message;
    Job::Builder builder = message.initRoot<Job>();
    builder.setId(job.id);
    builder.setWindow(job.window);
    builder.setDuration(job.duration);
    builder.setFinished(job.finished);
    builder.setStarttag(job.start_tag);
    builder.setStoptag(job.stop_tag);

    auto pats = builder.initPatterns(job.patterns.size());
    auto evts = builder.initEvents(job.events.size());

    for (unsigned long int i = 0; i < job.patterns.size(); ++i) {
        pats.set(i, job.patterns[i]);
        evts.set(i, job.events[i]);
    }

    auto msg = messageToFlatArray(message);
    auto msgarr_c = msg.asChars();

    boost::iostreams::stream< boost::iostreams::array_source > source (msgarr_c.begin(), msgarr_c.size());
    std::stringstream ss;
    boost::iostreams::filtering_streambuf<boost::iostreams::output> outStream;
    outStream.push(boost::iostreams::zstd_compressor());
    outStream.push(ss);
    boost::iostreams::copy(source, outStream);

    //endoce string base 64 to make it sqlilte insertable
    std::string blobdata = ss.str();
    unsigned int writePaddChars = (3-blobdata.length()%3)%3;
    std::string blobdata_base64(it_base64_t(blobdata.begin()),it_base64_t(blobdata.end()));
    blobdata_base64.append(writePaddChars,'=');

    //write to db
    int ret = 0;
    sqlite3_stmt *stmt;

    std::string sql_cmd = "INSERT INTO RESULTS VALUES (";
                sql_cmd.append(std::to_string(job.id));
                sql_cmd.append(", ");
                sql_cmd.append(job.finished ? "1" : "0");
                sql_cmd.append(", ?);");

    ret = sqlite3_prepare_v2(DB, sql_cmd.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        std::string errMsg(sqlite3_errmsg(DB));
        std::cout << "prepare error: \n" << errMsg << std::endl;
    }
    ret = sqlite3_bind_blob(stmt, 1, blobdata_base64.c_str(), blobdata_base64.size(), 0);
    if (ret != SQLITE_OK) {
        std::string errMsg(sqlite3_errmsg(DB));
        std::cout << "bind error: \n" << errMsg << std::endl;
    }
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW && ret != SQLITE_DONE) {
        std::string errMsg(sqlite3_errmsg(DB));
        std::cout << "step error: \n" << errMsg << std::endl;
        sqlite3_finalize(stmt);
    }
    if (ret == SQLITE_DONE) {
        sqlite3_finalize(stmt);
    }
    //sqlite3_finalize(stmt);
}

int kalidb::job_from_db(uint64_t id, jobstruct &out_job) {
    int ret=-1;
    std::string sql_cmd = "select * from results where id = ";
    sql_cmd.append(std::to_string(id));
    sql_cmd.append(";");

    sqlite3_stmt *stmt;
    ret = sqlite3_prepare_v2(DB, sql_cmd.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        std::string errmsg(sqlite3_errmsg(DB));
        std::cout << errmsg << std::endl;
        std::cout << ret << std::endl;
        return -1;
    }

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW && ret != SQLITE_DONE) {
        std::string errmsg(sqlite3_errmsg(DB));
        std::cout << errmsg << std::endl;
        sqlite3_finalize(stmt);
        return -1;
    }
    if (ret == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        std::cout << std::string("not found") << std::endl;
        return -2;
    }

    //uint64_t ret_id = sqlite3_column_int(stmt, 0);
    //bool ret_finished = sqlite3_column_int(stmt, 1) == 1 ? true : false;
    uint64_t ret_blobsize = sqlite3_column_bytes(stmt, 2);

    const unsigned char* p = sqlite3_column_text(stmt,2);

    std::string blob;
    blob.assign(p, p+ret_blobsize);

    sqlite3_finalize(stmt);

    //decode base64 string
    std::string blobdata(it_binary_t(blob.begin()),it_binary_t(blob.end()));

    //unzstd
    std::stringstream ss;
    ss << blobdata;
    char *buf = new char[20 * ret_blobsize];

    boost::iostreams::stream< boost::iostreams::array_sink > sink (buf, 20 * ret_blobsize * sizeof(char));
    boost::iostreams::filtering_streambuf<boost::iostreams::input> inStream;
    inStream.push(boost::iostreams::zstd_decompressor());
    inStream.push(ss);
    boost::iostreams::copy(inStream, sink);

    //read capnproto job message
    ::capnp::ReaderOptions opts;
    opts.traversalLimitInWords = 1.9 * 1024 * 1024 * 1024 ;

    kj::ArrayPtr<capnp::word> words(reinterpret_cast<capnp::word*>(buf), ret_blobsize / sizeof(char));
    ::capnp::FlatArrayMessageReader message(words, opts);
    Job::Reader reader = message.getRoot<Job>();

    //fill jobstruct
    for (uint16_t pat : reader.getPatterns()) {
        out_job.patterns.emplace_back(pat);
    }
    for (uint64_t evt : reader.getEvents()) {
        out_job.events.emplace_back(evt);
    }
    out_job.duration = reader.getDuration();
    out_job.duration_s = reader.getDuration() * tagger_info.resolution;
    out_job.finished = reader.getFinished();
    out_job.start_tag = reader.getStarttag();
    out_job.stop_tag = reader.getStoptag();
    out_job.window = reader.getWindow();
    out_job.id = reader.getId();

    delete [] buf;

    return 0;
}

void kalidb::set_tagger_info(tagger_info_struct &ti) {
    tagger_info.fpga_version = ti.fpga_version;
    tagger_info.num_inputs = ti.num_inputs;
    tagger_info.resolution = ti.resolution;
}
