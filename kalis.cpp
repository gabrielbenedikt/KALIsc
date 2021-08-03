#include "ttreader.h"

using namespace std;
using namespace TimeTag;


class TaggerImpl final: public Tagger::Server {
public:
    kj::Promise<void> savetags(SavetagsContext context) override {
        //savetags        @0 (filename :Text, chans :List(UInt8)          # to a specified filename
        //                    duration :UInt16)   -> (jobid :UInt64);     # for duration number of seconds

        tagfileinfo tf;

        tf.duration = ceil(context.getParams().getDuration() / tagger_info.resolution);
        tf.filename = context.getParams().getFilename();
        // overwrite old file
        std::filesystem::remove(tf.filename+raw_chn_ext);
        std::filesystem::remove(tf.filename+raw_tag_ext);
        tf.start_tag = 0;
        tf.stop_tag = 0;
        tf.finished = false;
        tf.converted = false;

        std::vector<uint8_t> chans;
        for (auto c : context.getParams().getChans()) {
            chans.push_back(c);
        }
        tf.channels = chans;

        std::random_device r;
        std::default_random_engine generator(r());
        std::uniform_int_distribution<uint64_t> chan_distribution(getnewid()+pow(10,3),std::numeric_limits<uint64_t>::max());
        auto rndull = std::bind ( chan_distribution, generator );
        tf.id = rndull();

        context.getResults().setJobid(tf.id);

        tagqueue_mtx.lock();
        tagfilequeue.push_back(tf);
        tagqueue_mtx.unlock();

        return kj::READY_NOW;
    }
    kj::Promise<void> submitjob(SubmitjobContext context) override {
        //submitjob       @1 (job: Jobdef)        -> (jobid :UInt64);
        uint64_t id = add_job(context.getParams().getJob());
        context.getResults().setJobid(id);
        return kj::READY_NOW;
    }
    kj::Promise<void> getresults(GetresultsContext context) override {
        //getresults      @2 (jobid :UInt64)      -> (payload :Jobdef);
        uint64_t id = context.getParams().getJobid();
        //check if id is in current joblist
        bool found = false;
        bool send = false;

        jobstruct jobtosend;
        auto payload = context.getResults().initPayload();
        job_mtx.lock();
        for (jobstruct job : jobs) {
            if (job.id == id) {
                found = true;
                if (job.finished) {
                    jobtosend = job;
                    send = true;
                } else {
                    payload.setErr("job not finished.");
                    job_mtx.unlock();
                    return kj::READY_NOW;
                }
            }
        }
        job_mtx.unlock();
        //check database
        if (!found) {
            int ret = job_from_db(id, jobtosend);
            if (ret == 0) {
                send = true;
            } else if (ret == -2) {
                return kj::READY_NOW;
            } else {
                payload.setErr("Error code "+std::to_string(ret));
                return kj::READY_NOW;
            }
        }

        if (send) {
            payload.setDuration(jobtosend.duration);
            payload.setErr("");
            payload.setFinished(jobtosend.finished);
            payload.setId(jobtosend.id);
            payload.setStarttag(jobtosend.start_tag);
            payload.setStoptag(jobtosend.stop_tag);
            payload.setWindow(jobtosend.window);
            auto evts = payload.initEvents(jobtosend.events.size());
            auto pats = payload.initPatterns(jobtosend.patterns.size());
            for (unsigned long int i = 0; i < pats.size(); ++i) {
                evts.set(i, jobtosend.events[i]);
                pats.set(i, jobtosend.patterns[i]);
            }
            return kj::READY_NOW;
        } else {
            payload.setErr("unknown error");
            return kj::READY_NOW;
        }
    }
    kj::Promise<void> queryjobdone(QueryjobdoneContext context) override {
        //queryjobdone    @2 (jobid :UInt64)      -> (ret :UInt8);
        uint64_t id = context.getParams().getJobid();

        context.getResults().setRet(returncodes::NOT_FOUND);

        //check for jobid in jobqueue
        job_mtx.lock();
        for (jobstruct job : jobs) {
            if (job.id == id) {
                if (job.finished) {
                    job_mtx.unlock();
                    context.getResults().setRet(returncodes::DONE);
                    return kj::READY_NOW;
                } else {
                    job_mtx.unlock();
                    context.getResults().setRet(returncodes::NOT_DONE);
                    return kj::READY_NOW;
                }
            }
        }
        job_mtx.unlock();

        //check for jobid in tagfilequeue
        tagqueue_mtx.lock();
        for (auto job : tagfilequeue) {
            if (job.id == id) {
                if (job.converted) {
                    tagqueue_mtx.unlock();
                    context.getResults().setRet(returncodes::DONE);
                    return kj::READY_NOW;
                } else {
                    tagqueue_mtx.unlock();
                    context.getResults().setRet(returncodes::NOT_DONE);
                    return kj::READY_NOW;
                }
            }
        }
        tagqueue_mtx.unlock();

        //check database
        jobstruct job;
        int ret = job_from_db(id, job);
        if (ret == 0) {
            context.getResults().setRet(returncodes::DONE);
            return kj::READY_NOW;
        } else if (ret == -2) {
            context.getResults().setRet(returncodes::NOT_FOUND);
            return kj::READY_NOW;
        } else {
            context.getResults().setRet(returncodes::UNKNOWN_ERROR);
            return kj::READY_NOW;
        }
    }
};

void del_job(uint64_t id) {
    std::cout << " in del_job" << std::endl;
    for (auto it=jobs.begin(); it!=jobs.end(); ++it) {
        if (it->id == id) {
            jobs.erase(it);
        }
    }
}

uint64_t add_job(const Job::Reader &reader) {
    jobstruct job;

    uint64_t id=getnewid();

    job.id=id;

    for (auto p : reader.getPatterns()) {
        job.patterns.push_back(p);
    }

    job.events.resize(job.patterns.size(), 0);
    job.window = reader.getWindow();
    job.duration = ceil(reader.getDuration() / tagger_info.resolution); // convert from seconds to internal ticks
    job.start_tag = 0;
    job.stop_tag = 0;
    job.finished = false;

    job_mtx.lock();
    jobs.push_back(job);
    job_mtx.unlock();

    print_job(job);

    return id;
}

void print_job(jobstruct &job) {
    std::cout << "Job\n";
    std::cout << "id: \t" << job.id << "\n";
    for (uint32_t i=0; i < job.patterns.size(); ++i) {
        std::cout << "pat: \t" << std::bitset<16>(job.patterns[i]) << "\t#: " << job.events[i] << "\n";
    }
    std::cout << "wnd: \t" << job.window << "\n";
    std::cout << "duration: \t" << job.duration << "\n";
    std::cout << "start tag: \t" << job.start_tag << "\n";
    std::cout << "stop tag: \t" << job.stop_tag << "\n";
    std::cout << "finished: \t" << job.finished << "\n";
    std::cout << std::endl;
}

int open_db() {
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

uint64_t getnewid() {
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

    // id_max is the latest ID in the database now
    // check if there are newer IDs in the current job queue
    uint64_t id_maxqueue = 0;
    for (auto j : jobs) {
        id_maxqueue = id_maxqueue > j.id ? id_maxqueue : j.id;
    }
    id_max = std::max(id_max, id_maxqueue);

    return id_max+1;
}

void job_to_db(jobstruct &job) {
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

int job_from_db(uint64_t id, jobstruct &out_job) {
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
    out_job.finished = reader.getFinished();
    out_job.start_tag = reader.getStarttag();
    out_job.stop_tag = reader.getStoptag();
    out_job.window = reader.getWindow();
    out_job.id = reader.getId();

    delete [] buf;

    return 0;
}


void signal_handler(int s){
    printf("Caught signal %d\n",s);
    shutdown();
}

void shutdown() {
    tt_close();
    exit(1);
}

int main() {
    // catch ctrl+c
    connect_signal_handler();
    
    tt_open();
    //tt_calibrate();
    
    tt_set_led_brightness(10);
    
    tt_get_fpga_version(tagger_info.fpga_version);
    tt_get_resolution(tagger_info.resolution);
    tt_get_num_inputs(tagger_info.num_inputs);
    
    print_info();
    
    
    if (0) {
        tt_set_frequency_generator_hz(10);
        while (true) {
            menu();
        }
    } else {
        tt_set_frequency_generator_hz(11000000);
        start_tag_server();
    }
    return 0;
}


/*************************************
 *************************************
 * 
 * menu / ui functions
 * 
 ****************
 ****************/

int menu() {
    printf("16 channel timetagger\n");
    printf("(a): Start tag server\n");
    printf("(p): Print timetags to stdout\n");
    printf("(s): Save raw tags to file\n");
    printf("(c): Convert raw tag file to text\n");
    printf("(r): Read tags and process tests\n");
    printf("(f): Set frequency generator frequency\n");
    printf("(q): Quit\n");
    
    char c = getchar();
    
    switch (toupper(c)) {
        case 'A':
            start_tag_server();
            break;
        case 'P':
            tt_print_tags();
            break;
        case 'S':
            tt_save_raw_tags("binarytags");
            break;
        case 'C':
            convert_raw_tags("binarytags", "tags");
            break;
        case 'R':
            test_tag_functions();
            break;
        case 'F':
            long freq;
            printf("Please enter frequency in Hz (1Hz...10MHz:)\n");
            cin >> freq;
            if (freq < 1) {
                freq = 1;
            } else if (freq > 10000000) {
                freq = 10000000;
            }
            tt_set_frequency_generator_hz(freq);
            break;
        case 'Q':
            shutdown();
            break;
    }
    
    return 0;
}

int print_info() {
    printf("Tagger info\n");
    printf("---------------------------\n");
    printf("FPGA version       : %i\n", tagger_info.fpga_version);
    printf("Resolution         : %e s\n", tagger_info.resolution);
    printf("Number of inputs   : %i\n", tagger_info.num_inputs);

    return 0;
}

int tt_print_tags() {
    // catch keyboard events
    std::thread keyboard_thread(kbd_interrupt);
    unsigned char *chan;
    long long *time;
    int count = 0;
    int j = 0;
    
    // kbd interrupt with q
    kbd_stop = false;
    std::ios_base::sync_with_stdio(false);
    tagger.StartTimetags();
    while (!kbd_stop) {
        count = tagger.ReadTags(chan, time);
        for (j = 0; j<count; ++j) {
            printf("chan %i \t tag %lli \n", (int)chan[j], time[j]);
        }
    }
    std::ios_base::sync_with_stdio(true);
    keyboard_thread.join();
    tagger.StopTimetags();
    
    return 0;
}

void kbd_interrupt() {
    printf("press q to stop\n");
    char c;
    cin >> c;
    switch (toupper(c)) {
        case 'Q':
            kbd_stop=true;
            break;
    }
}

void connect_signal_handler() {
   struct sigaction sigIntHandler;
   sigIntHandler.sa_handler = signal_handler;
   sigemptyset(&sigIntHandler.sa_mask);
   sigIntHandler.sa_flags = 0;
   sigaction(SIGINT, &sigIntHandler, NULL);
}


/*************************************
 *************************************
 * 
 * Tag functions
 * 
 ****************
 ****************/


/*
 * This function connects to the device. It has to be called before any other function is called.
 */
int tt_open() {
    printf("Connecting to timetagger...\n");
    #ifdef DEBUG_LIBUSB
    putenv( "LIBUSB_DEBUG=4" );
    #endif //DEBUG_LIBUSB
        
    try	{
        tagger.Open();
    } catch (TimeTag::Exception & ex)	{
        printf ("\nErr: %s\n with errno %d\n", ex.GetMessageText().c_str(), errno);
        printf (libusb_strerror(errno));
        return errno;
    }

    return 0;
}

/*
 * This function should be called before the program terminates.
 */
int tt_close() {
    try	{
        tagger.Close();
    } catch (TimeTag::Exception & ex)	{
        printf ("\nErr: %s\n with errno %d\n", ex.GetMessageText().c_str(), errno);
        printf (libusb_strerror(errno));
        return errno;
    }

    return 0;
}

/*
 * This function writes timetags to binary files.
 * Not portable - convert on same machine!
 */
int tt_save_raw_tags(std::string outfname) {
    std::ofstream outfile_chan (outfname+"_chan.tt", std::ofstream::binary);
    std::ofstream outfile_tags (outfname+"_tags.tt", std::ofstream::binary);
    std::thread keyboard_thread(kbd_interrupt);
    kbd_stop = false;

    long long *time;
    unsigned char* chan;
    int count = 0;
    
    kbd_stop = false;
    tagger.StartTimetags();

    while (!kbd_stop) {
        count = tagger.ReadTags(chan, time);
        outfile_chan.write(reinterpret_cast<char*>(&chan[0]), count);
        outfile_tags.write(reinterpret_cast<char*>(&time[0]), count*sizeof(long long));
        outfile_chan.flush();
        outfile_tags.flush();
    }
    
    keyboard_thread.join();
    tagger.StopTimetags();
    
    outfile_chan.close();
    outfile_tags.close();
    
    return 0;
}

/*
 * The calibration function increases the accuracy of the device. It needs 4-10 seconds to execute. When the
 * calibration is used, it should be the first function to be called after the “Open” function.
 * Note: Intrinsic delays of each input may vary after the calibration procedure is performed.
 */
int tt_calibrate() {
    printf("Timetagger calibration...\n");
    try	{
        tagger.Calibrate();
    } catch (TimeTag::Exception & ex)	{
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * The Brightness of the LED on the front panel can be changed.
 */
int tt_set_led_brightness(int percent) {
    printf("Setting timetagger LED brightness to %d percent\n", percent);
    
    //value sanity check
    if (percent < 0) {
        printf("value cannot be less than 0. Setting to 0.\n");
        percent = 0;
    }
    if (percent > 100) {
        printf("value cannot be greater than 100. Setting to 100.\n");
        percent = 100;
    }
    
    try {
        tagger.SetLedBrightness(percent);
    } catch (TimeTag::Exception & ex)	{
		printf("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function returns the current version of the FPGA design. It is used for debugging purposes only.
 */
int tt_get_fpga_version(int& out_version) {
    try	{
        out_version = tagger.GetFpgaVersion();
    } catch (TimeTag::Exception & ex)	{
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function returns the time resolution of the device. It should be used to calculate absolute time
 * values. The function returns either 78.125E-12 or 156.25E-12 seconds.
 */
int tt_get_resolution(float& out_resolution) {
    try	{
        out_resolution = tagger.GetResolution();
    } catch (TimeTag::Exception & ex)	{
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function returns the number of inputs installed on the device. It is used for debugging purposes only.
 */
int tt_get_num_inputs(int& out_inputs) {
    try	{
        out_inputs = tagger.GetNoInputs();
    } catch (TimeTag::Exception & ex)	{
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function sets the input voltage threshold per channel
 * 
 * chan: channel [1...16]
 * voltage: voltage threshold [-2...2]
 */
int tt_set_input_threshold(int chan, double voltage){
    // values sanity check
    if ((chan < 1) or (chan > 16)) {
        printf("channel has to be in [1, 16]\n");
        return -1;
    }
    if ((voltage < -2) or (voltage > 2)) {
        printf("voltage has to be in [-2, 2]\n");
        return -1;
    }
    
    try	{
        tagger.SetInputThreshold(chan, voltage);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * The inputs are edge sensitive. The positive edge is used as standard. With this function the relevant edge
 * can be changed to the negative edge. The mask is coded binary. When the corresponding bit is high, the
 * negative edge is used. The corresponding bits are bits 16 to 1. Bit 0 is unused.
 */
int tt_set_inversion_mask(int mask){
    // TODO: check if mask is meaningful
    try	{
        tagger.SetInversionMask(mask);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * All input signals can be delayed internally. This is useful to compensate external cable delays.
 * 
 * chan: channel [1...16]
 * delay: delay (18 bit value) delay in internal units
 */
int tt_set_delay(int chan, int delay){
    // values sanity check
    if ((chan < 1) or (chan > 16)) {
        printf("channel has to be in [1, 16]\n");
        return -1;
    }
    // TODO: check if delay is meaningful
    try	{
        tagger.SetDelay(chan, delay);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function defines the rectangular signal on connector output 4. Both values are defined in units of 5
 * ns. The maximum width of both values is 28 bit which gives a maximum time of about 1.3 sec.
 */
int tt_set_frequency_generator(int period, int high){
    // in units of 5 ns
    // (20,10) gives 10MHz signal with 50% duty cycle
    try	{
        tagger.SetFG(period, high);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function outputs a rectangular signal on connector output 4. 5ns high time. period hz
 * 
 */
int tt_set_frequency_generator_hz(int hz){
    // in units of 5 ns
    // (20,10) gives 10MHz signal with 50% duty cycle
    try	{
        int period = int(pow(10,9)/(5*hz));
        tagger.SetFG(period, 1);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * When the 10 MHz input is switched on, but no valid signal is connected to the input, an error flag is set
 * and the error led on the front panel is lit.
 * Note: When using the 10 MHz reference, input 16 on the 156.25 ps devices and input 8 on the 78.125 ps
 * devices cannot be used.
 */
int tt_use_ext_clock(bool use){
    try	{
        tagger.Use10MHz(use);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function stores all the single counters synchronously. This function also returns the time
 * between the last two calls to FreezeSingleCounter. The time is expressed in 5 ns ticks.
 */
int tt_freeze_single_counter(){
    try	{
        tagger.FreezeSingleCounter();
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This returns the number of input pulses in between the last two calls of FreezeSingleCounter.
 */
int tt_get_single_count(int chan){
    // values sanity check
    if ((chan < 1) or (chan > 16)) {
        printf("channel has to be in [1, 16]\n");
        return -1;
    }
    try	{
        tagger.GetSingleCount(chan);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function defines the minimum size of a group to be transmitted. MinCount can be set between 1 and
 * 10 counts. Setting MinCount to 1 switches the filter off, all tags are transmitted.
 */
int tt_set_filter_min_count(int min){
    // values sanity check
    if ((min < 1) or (min > 10)) {
        printf("filter min count has to be in [1, 10]\n");
        return -1;
    }
    try	{
        tagger.SetFilterMinCount(min);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * MaxTime defines the maximum time between two pulses in the same group. When the time between two
 * pulses is bigger than “MaxTime”, the two pulses are considered to be in different groups. MaxTime is
 * given in internal units.
 * Example: When FilterMinCount is 10 and FilterMaxTime is 1 μs, then the maximum possible group size
 * would be 9 μs.

 */
int tt_set_filter_max_time(int max){
    // values sanity check
    // given in internal units
    // 
    if (max < 0) {
        printf("filter max time has to be > 0\n");
        return -1;
    }
    try	{
        tagger.SetFilterMaxTime(max);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * Some inputs can be excluded from the filter. Excluded inputs are always transmitted. They do not
 * participate in groups. The filter exceptions are bit-coded. To exclude Input n from the filter, set the bit 
 * n-1 in the exception mask.
 */
int tt_set_filter_exception(int ex){
    try	{
        tagger.SetFilterException(ex);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * When the voltage on the gate Input is higher than the threshold voltage, then the device operates
 * normally: The time tags of all inputs are stored in internal RAM and transmitted via USB. When
 * the voltage is below the threshold voltage, then the input signals are ignored and no tags are
 * stored in internal RAM. The threshold voltage is the voltage of channel 9.
 * This feature is not present in all devices. This input has jitter and timing resolution of 5 ns.
 * 
 * False: Normal operation, the gate input will be ignored.
 * True: Level Gate Operation, tags are stored only when the gate input is high.

 */
int tt_use_level_gate(bool use){
    try	{
        tagger.UseLevelGate(use);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * Returns true, when the gate input is above the input threshold
 */
int tt_level_gate_active(){
    try	{
        tagger.LevelGateActive();
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This gating offers fine grained timing control and very low jitter. The width of the gating window
 * can be adjusted in steps of the internal resolution. The position of the gate can be adjusted too.
 * This offers very flexible control of the gate.
 * The gate is opened a fixed time after the active edge of input 8. This fixed time can be set with
 * SetDelay(8, delay);
 * The rising edge is the standard active edge. This can be change by SetInversionMask();
 * A negative gate delay is possible too. To achieve this, the delay of input 8 must be set to 0 and
 * the delay of all other inputs must be set to the magnitude of the desired delay value. The gate is
 * open for a fixed time interval. This interval can be adjusted in in internal units.
 * 
 * This function switches edge sensitive gating on or off.
 */
int tt_use_edge_gate(bool use){
    try	{
        tagger.UseTimetagGate(use);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function sets the width of the gate. The parameter duration is given in internal units.
 */
int tt_set_edge_gate_width(int width){
    try	{
        tagger.SetGateWidth(width);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}


/*************************************
 *************************************
 * 
 * Error functions
 * 
 ****************
 ****************/

/*
 * This function returns the internal error flags.Calling this function clears the flags in the device.
 */
int tt_read_error_flags(){
    try	{
        int ec = tagger.ReadErrorFlags();
        if (ec) {
            printf("\nError code: %i\n", ec);
        }
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function translates the error flags to a short text that can be displayed on the user interface.
 *
int tt_get_error_text(int flags){
    try	{
        //tagger.GetErrorText(flags);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}
*/
/*************************************
 *************************************
 * 
 * Logic functions
 * 
 ****************
 ****************/

/*
 * This method must be called before the other logic functions can be used.
 */
int ttlogic_switch_logic_mode(){
    try	{
        ttlogic.SwitchLogicMode();
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This method sets width of the coincidence window. The parameter window is given in internal units
 * (156.25 ps or 78.125 ps). The window width can be set as high as 2 24-1.
 */
int ttlogic_set_window_width(int width){
    try	{
        ttlogic.SetWindowWidth(width);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This method reads the counter out of the device. The return parameter is not needed (used for debug
 * purposes only). The data is automatically stored in the Logic object for later processing. This method can
 * be called any time after calling SwitchLogicMode(). The time between calls to ReadLogic()
 * defines the measurement time interval of the captured data.
 * Note: The device uses a double buffered memory system. The new measurement starts immediately and
 * is running in the background during the call to ReadLogic(). For this reason not a single pulse is omitted
 * during readout.
 */
int ttlogic_read_logic(){
    try	{
        ttlogic.ReadLogic();
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This method operates on the data read by the last call to ReadLogic(). It calculates how often a certain
 * event pattern has occurred in the last measurement interval. The parameters pos and neg are bit coded.
 * The rightmost bit corresponds to input 1, the next to input 2, and so on.
 * The parameter pos is given by the integer value of the binary number defined by the bit coded
 * Coincidence Event. It defines the inputs that must have an active edge in the coincidence window for the
 * event to be counted, with range [0..65535].
 * The parameter neg defines the inputs that must have no active edge in the coincidence window for the
 * pattern to be counted. The parameter neg is optional, with range [0..65535].
 */
int ttlogic_calc_count(int pos, int neg){
    try	{
        ttlogic.CalcCount(pos, neg);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This method equals CalcCount except that the neg parameter is always 0. It has better run time.
 */
int ttlogic_calc_count_pos(int pos){
    try	{
        ttlogic.CalcCountPos(pos);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * The time counter measures the time between the last two calls to ReadLogic. The result is given in
 * multiples of 5 ns. The time when ReadLogic() is called will vary over time because of the limited realtime
 * performance of personal computers. The correct count rates can be obtained when the count values from
 * CalcCount are divided by the result from GetTimeCounter().
 */
int ttlogic_get_time_counter(){
    try	{
        ttlogic.GetTimeCounter();
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function is an advanced feature that can be used to fine tune the delay of the outputs. Roughly
 * speaking, events is the number of events you expect to occur in one 5 ns time slice under worst-case
 * conditions.
 * Increasing the value by one will increase the delay of the outputs by 10 ns. With the standard setting of 5
 * the output delay will be 350 ns.
 * When the input rate is too high for the given event count, the OutTooLate Error Flag will be raised. No
 * output pulse will be generated in this condition. There is never an output pulse generated in the wrong
 * timing. For this reason OutTooLate is more a kind of warning, not a hard error condition. It just means
 * that not all pulses are generated.
 * Hint: When two events occur at the exact same time, the event coming from the input with the smaller
 * input number will be processed first. For this reason input 1 has a slightly better real time performance
 * than input 16.
 */
int ttlogic_set_output_event_count(int events){
    try	{
        ttlogic.SetOutputEventCount(events);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function sets the pattern of the output pulses based on the input coincidence events.
 * 
 * output: identifies which output to change, with range [1..3].
 * pos: is the bit coded value of the Coincidence Event of the inputs that must be present, with range [0..65535] as for CalcCount.
 * neg: is the bit coded value of the Coincidence Event of the inputs that must not be present, with range [0..65535].
 */
int ttlogic_set_output_pattern(int output, int pos, int neg){
    try	{
        ttlogic.SetOutputPattern(output, pos, neg);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}

/*
 * This function defines the length of the generated output pulses. The parameter is given in 5 ns increments.
 * The maximum value is 255.
 */
int ttlogic_set_output_width(int width){
    try	{
        ttlogic.SetOutputWidth(width);
    } catch (TimeTag::Exception & ex) {
		printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        return errno;
    }
    
    return 0;
}


/*************************************
 *************************************
 * 
 * Data processing
 * 
 ****************
 ****************/

/*
 * This function converts tags from internal time to nanoseconds
 */
int tags_to_ns(std::vector<long long> &tags, std::vector<double> &out_ns) {
    out_ns.resize(tags.size());
    std::transform(tags.begin(), tags.end(), out_ns.begin(), [](auto& e ) { return  e*tagger_info.resolution *pow(10,9); });
    return 0;
}

/*
 * This function converts tags in a binary file to a tab separated text file
 */
int convert_raw_tags(std::string infname, std::string outfname) {
    std::ifstream infile_chan (infname+"_chan.tt", std::ifstream::binary | std::ios::ate);
    std::ifstream infile_tags (infname+"_tags.tt", std::ifstream::binary | std::ios::ate);
    std::ofstream outfile (outfname+".tt", std::ofstream::out);
    
    // get size of file
    long long count_chan = infile_chan.tellg();
    long long count_tags = infile_tags.tellg();
    infile_chan.seekg (0, std::ios::beg);
    infile_tags.seekg (0, std::ios::beg);
    
    //create buffer
    char *chan = new char [count_chan];
    long long *time = new long long [count_tags];
    infile_chan.read(reinterpret_cast<char*>(&chan[0]),count_chan);
    infile_tags.read(reinterpret_cast<char*>(&time[0]),count_tags*sizeof(long long));
    
    for (long long i=0; i<count_chan; ++i) {
      outfile << (int)(chan[i]) << "\t" << (long long)time[i] << "\n";
    }
    
    infile_chan.close();
    infile_tags.close();
    outfile.close();

    delete [] chan;
    delete [] time;
    
    return 0;
}

/*
 * This function reads tags from a binary files into vectors
 */
int read_raw_tags(std::string fname, std::vector<int> &out_chan, std::vector<long long> &out_tags) {
    std::ifstream infile_chan (fname+"_chan.tt", std::ifstream::binary | std::ios::ate);
    std::ifstream infile_tags (fname+"_tags.tt", std::ifstream::binary | std::ios::ate);
    
    // get size of file
    long long count_chan = infile_chan.tellg();
    long long count_tags = infile_tags.tellg();
    infile_chan.seekg (0, std::ios::beg);
    infile_tags.seekg (0, std::ios::beg);
    
    //create buffer
    char *chan = new char [count_chan];
    long long *time = new long long [count_tags];
    infile_chan.read(reinterpret_cast<char*>(&chan[0]),count_chan);
    infile_tags.read(reinterpret_cast<char*>(&time[0]),count_tags*sizeof(long long));
    
    out_chan = std::vector<int>(chan, chan+count_chan);
    out_tags = std::vector<long long>(time, time+count_chan);
    
    infile_chan.close();
    infile_tags.close();
    delete [] chan;
    delete [] time;
    
    return 0;
}

/*
 * This function reads tags from a tab separated text file into vectors
 */
int read_text_tags(std::string fname, std::vector<int> &out_chan, std::vector<long long> &out_tags) {
    std::ifstream infile (fname+".tt");
    int chan;
    long long tag;
    
    while (infile >> chan >> tag) {
        out_chan.push_back(chan);
        out_tags.push_back(tag);
    }
    infile.close();
    
    return 0;
}

/*
 * calculates the average rate present in a tag vector
 * assumes tag vector in ns
 */
int average_rate(std::vector<double> &tags) {
    long long numtags = tags.size();
    
    double firsttag = tags[0];
    double lasttag  = tags[numtags-1];
    double diff = lasttag - firsttag;
    
    printf("firsttag %f\n", firsttag);
    printf("lasttag %f\n", lasttag);
    
    printf("\nRate\n");
    printf("----------------\n");
    printf("diff   : %fns\n", diff);
    printf("numtags: %lli\n", numtags);
    printf("rate   : %fHz\n", numtags/(diff*pow(10,-9)));
    
    return 0;
}

/*
 * This function writes data from the tagger to a capnproto file
 * Input in std::vectors
 */
void write_capnp_tags(std::string fname, std::vector<int> &channels, std::vector<long long> &tags) {
    ::capnp::MallocMessageBuilder message;
    TTdata::Builder ttdata = message.initRoot<TTdata>();
    
    // for List(type)
    //auto cp_chan = ttdata.initChan(channels.size());
    //auto cp_tags = ttdata.initTag(channels.size());
    //for List(List(type))

    // For List(List(UInt8)), List(List((UInt64))
    auto cp_chan_outer = ttdata.initChan(1);
    auto cp_tags_outer = ttdata.initTag(1);

    auto cp_chan = cp_chan_outer.init(0, channels.size());
    auto cp_tags = cp_tags_outer.init(0, tags.size());

    for (long unsigned int i=0; i<channels.size(); ++i) {
        cp_chan.set(i, channels[i]);
        cp_tags.set(i, tags[i]);
    }
    
    int fd = open(fname.append(".cpdat").c_str(), O_RDWR | O_CREAT, 0666);
    writeMessageToFd(fd, message);
    close(fd);
}

/*
 * This function reads data from a capnproto file
 * Output in std::vectors
 */
void read_capnp_tags(std::string fname, std::vector<int> &out_chan, std::vector<long long> &out_tags) {
    out_chan.clear();
    out_tags.clear();
    
    // by default, the capnproto rejects too large messages (for security reasons)
    // let us increase this limit here
    ::capnp::ReaderOptions opts;
    opts.traversalLimitInWords = 1.9 * 1024 * 1024 * 1024 ;
    
    int fd = open(fname.append(".cpdat").c_str(), O_RDONLY);
    ::capnp::StreamFdMessageReader message(fd, opts);
    close(fd);

    TTdata::Reader ttdata = message.getRoot<TTdata>();
    /*List(type)
    for (int chan : ttdata.getChan()) {
        out_chan.emplace_back(chan);
    }
    for (long long tag : ttdata.getTag()) {
        out_tags.emplace_back(tag);
    }
    */
    //List(List(type))
    for (size_t i = 0; i < ttdata.getChan().size(); ++i){
        for (int chan : ttdata.getChan()[i]) {
            out_chan.emplace_back(chan);
        }
        for (long long tag : ttdata.getTag()[i]) {
            out_tags.emplace_back(tag);
        }
    }

}

/*
 * This function writes data from the tagger to a zstd compressed capnproto file
 * Input in std::vectors
 */
void write_capnp_tags_compressed(std::string fname, std::vector<int> &channels, std::vector<long long> &tags) {
    ::capnp::MallocMessageBuilder message;
    TTdata::Builder ttdata = message.initRoot<TTdata>();
    
    /*List(type)
    auto cp_chan = ttdata.initChan(channels.size());
    auto cp_tags = ttdata.initTag(channels.size());
    */
    //List(List(type))
    auto cp_chan_outer = ttdata.initChan(1);
    auto cp_tags_outer = ttdata.initTag(1);
    auto cp_chan = cp_chan_outer.init(0, channels.size());
    auto cp_tags = cp_tags_outer.init(0, channels.size());
    for (long unsigned int i=0; i<channels.size(); ++i) {
        cp_chan.set(i, channels[i]);
        cp_tags.set(i, tags[i]);
    }
    
    // Instead of writing the message to disk, loading it, compressing it, and writing to disk again,
    // compress the message directly and write to disk
    auto msg = messageToFlatArray(message);
    auto msgarr_c = msg.asChars();

    boost::iostreams::stream< boost::iostreams::array_source > source (msgarr_c.begin(), msgarr_c.size());
    std::ofstream ofs (fname+".cpdat.zstd", std::ios::out | std::ios::binary); 
    boost::iostreams::filtering_streambuf<boost::iostreams::output> outStream; 
    outStream.push(boost::iostreams::zstd_compressor()); 
    outStream.push(ofs); 
    boost::iostreams::copy(source, outStream); 
}

/*
 * This function reads data from a capnproto file
 * Output in std::vectors
 */
void read_capnp_tags_compressed(std::string fname, std::vector<int> &out_chan, std::vector<long long> &out_tags) {
    out_chan.clear();
    out_tags.clear();
    // to create stream buffer, we need the filesize
    std::ifstream ifs (fname+".cpdat.zstd", std::ios::in | std::ios::binary | std::ios::ate); 
    long long size = ifs.tellg();
    ifs.seekg (0, std::ios::beg); 
    //auto buf = kj::heapArray<capnp::word>(size);
    //std::vector<capnp::word> buf(size);
    //std::vector<char> buf(size);
    char *buf = new char[20 * size];
    //boost::iostreams::stream< boost::iostreams::array_sink > sink (buf.begin(), buf.size() * sizeof(char));
    boost::iostreams::stream< boost::iostreams::array_sink > sink (buf, 20 * size * sizeof(char));
    boost::iostreams::filtering_streambuf<boost::iostreams::input> inStream;
    inStream.push(boost::iostreams::zstd_decompressor());
    inStream.push(ifs);
    boost::iostreams::copy(inStream, sink);
    ifs.close();
    
    // by default, the capnproto rejects too large messages (for security reasons)
    // let us increase this limit here
    ::capnp::ReaderOptions opts;
    opts.traversalLimitInWords = 1.9 * 1024 * 1024 * 1024 ;

    kj::ArrayPtr<capnp::word> words(reinterpret_cast<capnp::word*>(buf), size / sizeof(char));
    ::capnp::FlatArrayMessageReader message(words, opts);
    TTdata::Reader ttdata = message.getRoot<TTdata>();
    /*List(type)
    for (int chan : ttdata.getChan()) {
        out_chan.emplace_back(chan);
    }
    for (long long tag : ttdata.getTag()) {
        out_tags.emplace_back(tag);
    }
    */
    //List(List(type))
    for (size_t i=0; i<ttdata.getChan().size(); ++i){
        for (int chan : ttdata.getChan()[i]) {
            out_chan.emplace_back(chan);
        }
        for (long long tag : ttdata.getTag()[i]) {
            out_tags.emplace_back(tag);
        }
    }
    // This gives a double-free error. Maybe delete[] is happening somewhere in kj
    // I'll leave it as comment here bcs it's not obvious that it's right NOT to do it
    // Or maybe it's supposed to be here and debugging pointed me in the wrong direction.
    delete [] buf;
}

int send_tags_over_net () {
    // get tags
    // build message
    // compress
    // send
    
    
    
    // catch keyboard events
    unsigned char *chan;
    long long *time;
    int count = 0;
    
    // kbd interrupt with q
    std::thread keyboard_thread(kbd_interrupt);
    kbd_stop = false;
    std::ios_base::sync_with_stdio(false);
    tagger.StartTimetags();
    
    while (!kbd_stop) {
        // Get tags
        count = tagger.ReadTags(chan, time);
        
        // append to buffer
        tagbuf_mtx.lock();
        std::copy(chan, chan+count, std::back_inserter(chan_buf));
        std::copy(time, time+count, std::back_inserter(tag_buf));
        tagbuf_mtx.unlock();
        //printf("elements in chan_buf: %li \t elements in tag_buf: %li\n", chan_buf.size(), tag_buf.size());
        //printf("chan_buf.back: %i \t tag_buf.back(): %lli\n", chan_buf.back(), tag_buf.back());
        
        // buffer will be sent and emptied on request
        
    }
    std::ios_base::sync_with_stdio(true);
    keyboard_thread.join();
    tagger.StopTimetags();
    
    return 0;
}



int tagbuf_to_cap(TTdata::Builder &plb) {
    tagbuf_mtx.lock();
    
    /*List(type)
    auto cp_chan = plb.initChan(chan_buf.size());
    auto cp_tags = plb.initTag(chan_buf.size());
    
    for (long unsigned int i=0; i<chan_buf.size(); ++i) {
        cp_chan.set(i, chan_buf[i]);
        cp_tags.set(i, tag_buf[i]);
    }
    */
    //List(List(type))
    auto cp_chan_outer = plb.initChan(1);
    auto cp_tags_outer = plb.initTag(1);
    auto cp_chan = cp_chan_outer.init(0, chan_buf.size());
    auto cp_tags = cp_tags_outer.init(0, chan_buf.size());
    for (long unsigned int i=0; i<chan_buf.size(); ++i) {
        cp_chan.set(i, chan_buf[i]);
        cp_tags.set(i, tag_buf[i]);
    }
    chan_buf.clear();
    tag_buf.clear();
    
    tagbuf_mtx.unlock();
    
    return 0;
}

int start_tag_server() {
    bool use_two_threads = true;
    if (open_db() != 0) {
        return -1;
    }

    if (use_two_threads) {
        std::thread fill_bufs_thread(fill_bufs);
        std::thread tag_process_thread(process_tags);
        ::capnp::ReaderOptions opts;
        opts.traversalLimitInWords = 1.9 * 1024 * 1024 * 1024 ;
        capnp::EzRpcServer server(kj::heap<TaggerImpl>(), "*", PORT, opts);
        std::cout << "Listening" << std::endl;
        kj::NEVER_DONE.wait(server.getWaitScope());
        fill_bufs_thread.join();
        tag_process_thread.join();
    } else {
        std::thread tag_process_thread2(process_tags2);
        ::capnp::ReaderOptions opts;
        opts.traversalLimitInWords = 1.9 * 1024 * 1024 * 1024 ;
        capnp::EzRpcServer server(kj::heap<TaggerImpl>(), "*", PORT, opts);
        std::cout << "Listening" << std::endl;
        kj::NEVER_DONE.wait(server.getWaitScope());
        tag_process_thread2.join();
    }

    return 0;
}

void fill_bufs() {
    unsigned char *chan;
    long long *time;
    int count = 0;
    int j = 0;

    tagger.StartTimetags();
    while (true) {
        tt_read_error_flags();
        tagbuf_mtx.lock();

        count = tagger.ReadTags(chan, time);
        std::cout << count << std::endl;
        std::vector<int> tmp_chan_buf (chan, chan + count);
        std::vector<long long> tmp_tag_buf (time, time + count);
        chan_buf.insert(chan_buf.end(), tmp_chan_buf.begin(), tmp_chan_buf.end());
        tag_buf.insert(tag_buf.end(), tmp_tag_buf.begin(), tmp_tag_buf.end());

        //printf ("count: %i \n len tmp_chan_bf: %li \n len tmp_tag_buf: %li \n", count, tmp_chan_buf.size(), tmp_tag_buf.size());

        tagbuf_mtx.unlock();
    }
    tagger.StopTimetags();
}

void process_tags() {
    bool bufsupdated = false;
    std::vector<int> chans;
    std::vector<long long int> tags;
    while (true) {
        bufsupdated = false;
        tagbuf_mtx.lock();
        if (chan_buf.size() > 0) {
            chans = chan_buf;
            tags = tag_buf;
            chan_buf.clear();
            tag_buf.clear();
            bufsupdated = true;
        }
        tagbuf_mtx.unlock();

        if (bufsupdated) {
            std::cout << "process" << std::endl;
            job_mtx.lock();
            std::cout << "joblock" << std::endl;
            auto job = jobs.begin();
            while (job != jobs.end()) {
                //std::cout << "client active? ..." << std::endl;
                if (!job->finished) {
                    //std::cout << "client active" << std::endl;
                    int64_t curr_dur = job->stop_tag - job->start_tag;
                    if (job->start_tag == 0) {
                        job->start_tag = tags[0];
                    } else if (curr_dur > job->duration) {
                            job->finished = true;
                    }

                    for (uint32_t i = 0; i<job->patterns.size(); ++i) {
                        std::vector<uint8_t> pat_chans = helpers::bitmask_to_channels(job->patterns[i]);
                        switch (pat_chans.size()) {
                        case 1:
                            job->events[i] += std::count(chans.begin(), chans.end(), pat_chans[0]);
                            break;
                        case 2:
                            //calc_2f();
                            break;
                        case 3:
                            //calc_3f();
                            break;
                        case 4:
                            //calc_4f();
                            break;
                        case 5:
                            //calc_5f();
                            break;
                        case 6:
                            //calc_6f();
                            break;
                        }
                    }
                    job->stop_tag = tags.back();
                    ++job;
                } else { // job finished. push to db
                    std::cout<<"jobtodb"<<std::endl;
                    job_to_db(*job);
                    std::cout<<"jobtodbdone"<<std::endl;
                    job = jobs.erase(job);
                }
            }
            job_mtx.unlock();
            std::cout << "jobunlock" << std::endl;

            tagqueue_mtx.lock();
            for (auto tagjob = tagfilequeue.begin(); tagjob != tagfilequeue.end(); ++tagjob) {
                //std::cout << "client active? ..." << std::endl;
                if (!tagjob->finished) {
                    int64_t curr_dur = tagjob->stop_tag - tagjob->start_tag;
                    if (tagjob->start_tag == 0) {
                        tagjob->start_tag = tags[0];
                    // TODO: write note on why ignoring this warning is fine
                    } else if (curr_dur > tagjob->duration) {
                            tagjob->finished = true;
                    }

                    // TODO: tags to files
                    std::ofstream ofs_chns (tagjob->filename+raw_chn_ext, std::ifstream::binary | std::ios::app);
                    std::ofstream ofs_tags (tagjob->filename+raw_tag_ext, std::ifstream::binary | std::ios::app);

                    /*
                    //save only channels of interest
                    std::vector<int> idxs;
                    std::vector<int>::iterator it = chans.begin();
                    while ((it= std::find_if(it, chans.end(), [&tj](int x){return std::find(tagjob->channels.begin().begin(), tagjob->channels.begin().end(), x; })) != chans.end())
                    {
                        idxs.push_back(std::distance(chans.begin(), it));
                        it++;
                    }
                    std::vector<int> chans_to_save;
                    std::vector<long long int> tags_to_save;
                    for (auto i : idxs) {
                        chans_to_save.push_back(chans[i]);
                        tags_to_save.push_back(tags[i]);
                    }
                    */
                    ofs_chns.write(reinterpret_cast<char*>(&chans[0]), chans.size());
                    ofs_tags.write(reinterpret_cast<char*>(&tags[0]), tags.size()*sizeof(long long));
                    ofs_chns.flush();
                    ofs_tags.flush();
                    ofs_chns.close();
                    ofs_tags.close();
                    tagjob->stop_tag = tags.back();
                    } else {
                        if (!tagjob->converted) {
                            //read back full raw tag files
                            std::ifstream infile_chan (tagjob->filename+raw_chn_ext, std::ifstream::binary | std::ios::ate);
                            std::ifstream infile_tags (tagjob->filename+raw_tag_ext, std::ifstream::binary | std::ios::ate);
                            long long count_chan = infile_chan.tellg();
                            long long count_tags = infile_tags.tellg();
                            infile_chan.seekg (0, std::ios::beg);
                            infile_tags.seekg (0, std::ios::beg);

                            //create buffer
                            char *chan = new char [count_chan];
                            long long *time = new long long [count_tags];
                            infile_chan.read(reinterpret_cast<char*>(&chan[0]),count_chan);
                            infile_tags.read(reinterpret_cast<char*>(&time[0]),count_tags*sizeof(long long));

                            std::vector<uint8_t> channels = std::vector<uint8_t>(chan, chan+count_chan);
                            std::vector<long long int> tags = std::vector<long long int>(time, time+count_chan);

                            infile_chan.close();
                            infile_tags.close();
                            delete [] chan;
                            delete [] time;

                            //write compressed capnproto message
                            ::capnp::MallocMessageBuilder message;
                            TTdata::Builder ttdata = message.initRoot<TTdata>();

                            // For List(List(UInt8)), List(List((UInt64))
                            auto cp_chan_outer = ttdata.initChan(1);
                            auto cp_tags_outer = ttdata.initTag(1);

                            auto chanlist = cp_chan_outer.init(0, channels.size());
                            auto taglist = cp_tags_outer.init(0, tags.size());

                            for (long unsigned int i=0; i<channels.size(); ++i) {
                                chanlist.set(i,channels[i]);
                                taglist.set(i, tags[i]);
                            }

                            /* For List(UInt8), List(UInt64)
                            auto cp_chan = ttdata.initChan(channels.size());
                            auto cp_tags = ttdata.initTag(channels.size());

                            for (long unsigned int i=0; i<channels.size(); ++i) {
                                cp_chan.set(i, channels[i]);
                                cp_tags.set(i, tags[i]);
                            }
                            */

                            // Instead of writing the message to disk, loading it, compressing it, and writing to disk again,
                            // compress the message directly and write to disk
                            auto msg = messageToFlatArray(message);
                            auto msgarr_c = msg.asChars();

                            boost::iostreams::stream< boost::iostreams::array_source > source (msgarr_c.begin(), msgarr_c.size());
                            std::ofstream ofs (tagjob->filename+".cpdat.zstd", std::ios::out | std::ios::binary);
                            boost::iostreams::filtering_streambuf<boost::iostreams::output> outStream;
                            outStream.push(boost::iostreams::zstd_compressor());
                            outStream.push(ofs);
                            boost::iostreams::copy(source, outStream);

                            tagjob->converted = true;

                            std::filesystem::remove(tagjob->filename+raw_chn_ext); // delete file
                            std::filesystem::remove(tagjob->filename+raw_tag_ext); // delete file
                        } else {

                        }
                }
            }
            tagqueue_mtx.unlock();
        } else {
            // if tagbuf is empty, wait for a few ms to not continuously lock the mutex
            //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    std::cout << "exit process tag loop" << std::endl;
}


void process_tags2() {

    unsigned char *chan;
    long long *time;
    int count = 0;
    int j = 0;
    bool bufsupdated = false;

    tagger.StartTimetags();

    //std::cout << " in process_tags" << std::endl;
    while (true) {

        tagbuf_mtx.lock();
        try {
            count = tagger.ReadTags(chan, time);
        } catch (TimeTag::Exception ex) {
            printf ("\nErr: %s\n", ex.GetMessageText().c_str());
        } catch (std::exception ex) {
            printf ("\nErr: %s\n", ex.what());
        }
        std::vector<int> tmp_chan_buf (chan, chan + count);
        std::vector<long long> tmp_tag_buf (time, time+count);

        chan_buf.insert(chan_buf.end(), tmp_chan_buf.begin(), tmp_chan_buf.end());
        tag_buf.insert(tag_buf.end(), tmp_tag_buf.begin(), tmp_tag_buf.end());

        bufsupdated = chan_buf.size() > 0 ? true : false;

        tagbuf_mtx.unlock();

        if (bufsupdated) {
            std::cout << "size of chan_buf: " << chan_buf.size() << "\n";
            std::cout << "size of tag_buf: " << tag_buf.size() << "\n";
            std::cout << "process" << std::endl;
            job_mtx.lock();
            auto job = jobs.begin();
            while (job != jobs.end()) {
                //std::cout << "client active? ..." << std::endl;
                if (!job->finished) {
                    //std::cout << "client active" << std::endl;
                    int64_t curr_dur = job->stop_tag - job->start_tag;
                    if (job->start_tag == 0) {
                        job->start_tag = tag_buf[0];
                    } else if (curr_dur > job->duration) {
                            job->finished = true;
                    }

                    // negative tags suck balls. but this comparison shouls still work
                    for (uint32_t i = 0; i<job->patterns.size(); ++i) {
                        std::vector<uint8_t> pat_chans = helpers::bitmask_to_channels(job->patterns[i]);
                        switch (pat_chans.size()) {
                        case 1:
                            job->events[i] += std::count(chan_buf.begin(), chan_buf.end(), pat_chans[0]);
                            break;
                        case 2:
                            //calc_2f();
                            break;
                        case 3:
                            //calc_3f();
                            break;
                        case 4:
                            //calc_4f();
                            break;
                        case 5:
                            //calc_5f();
                            break;
                        case 6:
                            //calc_6f();
                            break;
                        }
                    }
                    job->stop_tag = tag_buf.back();
                    ++job;
                } else { // job finished. push to db
                    std::cout<<"jobtodb"<<std::endl;
                    job_to_db(*job);
                    std::cout<<"jobtodbdone"<<std::endl;
                    job = jobs.erase(job);
                }
            }
            job_mtx.unlock();

            tagqueue_mtx.lock();
            for (auto tagjob = tagfilequeue.begin(); tagjob != tagfilequeue.end(); ++tagjob) {
                //std::cout << "client active? ..." << std::endl;
                if (!tagjob->finished) {
                    int64_t curr_dur = tagjob->stop_tag - tagjob->start_tag;
                    if (tagjob->start_tag == 0) {
                        tagjob->start_tag = tag_buf[0];
                    // TODO: write note on why ignoring this warning is fine
                    } else if (curr_dur > tagjob->duration) {
                            tagjob->finished = true;
                    }

                    // TODO: tags to files
                    std::ofstream ofs_chns (tagjob->filename+raw_chn_ext, std::ifstream::binary | std::ios::app);
                    std::ofstream ofs_tags (tagjob->filename+raw_tag_ext, std::ifstream::binary | std::ios::app);

                    /*
                    //save only channels of interest
                    std::vector<int> idxs;
                    std::vector<int>::iterator it = chans.begin();
                    while ((it= std::find_if(it, chans.end(), [&tj](int x){return std::find(tagjob->channels.begin().begin(), tagjob->channels.begin().end(), x; })) != chans.end())
                    {
                        idxs.push_back(std::distance(chans.begin(), it));
                        it++;
                    }
                    std::vector<int> chans_to_save;
                    std::vector<long long int> tags_to_save;
                    for (auto i : idxs) {
                        chans_to_save.push_back(chans[i]);
                        tags_to_save.push_back(tags[i]);
                    }
                    */
                    ofs_chns.write(reinterpret_cast<char*>(&chan_buf[0]), chan_buf.size());
                    ofs_tags.write(reinterpret_cast<char*>(&tag_buf[0]), tag_buf.size()*sizeof(long long));
                    ofs_chns.flush();
                    ofs_tags.flush();
                    ofs_chns.close();
                    ofs_tags.close();
                    tagjob->stop_tag = tag_buf.back();
                    } else {
                        if (!tagjob->converted) {
                            //read back full raw tag files
                            std::ifstream infile_chan (tagjob->filename+raw_chn_ext, std::ifstream::binary | std::ios::ate);
                            std::ifstream infile_tags (tagjob->filename+raw_tag_ext, std::ifstream::binary | std::ios::ate);
                            long long count_chan = infile_chan.tellg();
                            long long count_tags = infile_tags.tellg();
                            infile_chan.seekg (0, std::ios::beg);
                            infile_tags.seekg (0, std::ios::beg);

                            //create buffer
                            char *chan = new char [count_chan];
                            long long *time = new long long [count_tags];
                            infile_chan.read(reinterpret_cast<char*>(&chan[0]),count_chan);
                            infile_tags.read(reinterpret_cast<char*>(&time[0]),count_tags*sizeof(long long));

                            std::vector<uint8_t> channels = std::vector<uint8_t>(chan, chan+count_chan);
                            std::vector<long long int> tags = std::vector<long long int>(time, time+count_chan);

                            infile_chan.close();
                            infile_tags.close();
                            delete [] chan;
                            delete [] time;

                            //write compressed capnproto message
                            ::capnp::MallocMessageBuilder message;
                            TTdata::Builder ttdata = message.initRoot<TTdata>();

                            // For List(List(UInt8)), List(List((UInt64))
                            auto cp_chan_outer = ttdata.initChan(1);
                            auto cp_tags_outer = ttdata.initTag(1);

                            auto chanlist = cp_chan_outer.init(0, channels.size());
                            auto taglist = cp_tags_outer.init(0, tags.size());

                            for (long unsigned int i=0; i<channels.size(); ++i) {
                                chanlist.set(i,channels[i]);
                                taglist.set(i, tags[i]);
                            }

                            /* For List(UInt8), List(UInt64)
                            auto cp_chan = ttdata.initChan(channels.size());
                            auto cp_tags = ttdata.initTag(channels.size());

                            for (long unsigned int i=0; i<channels.size(); ++i) {
                                cp_chan.set(i, channels[i]);
                                cp_tags.set(i, tags[i]);
                            }
                            */

                            // Instead of writing the message to disk, loading it, compressing it, and writing to disk again,
                            // compress the message directly and write to disk
                            auto msg = messageToFlatArray(message);
                            auto msgarr_c = msg.asChars();

                            boost::iostreams::stream< boost::iostreams::array_source > source (msgarr_c.begin(), msgarr_c.size());
                            std::ofstream ofs (tagjob->filename+".cpdat.zstd", std::ios::out | std::ios::binary);
                            boost::iostreams::filtering_streambuf<boost::iostreams::output> outStream;
                            outStream.push(boost::iostreams::zstd_compressor());
                            outStream.push(ofs);
                            boost::iostreams::copy(source, outStream);

                            tagjob->converted = true;

                            std::filesystem::remove(tagjob->filename+raw_chn_ext); // delete file
                            std::filesystem::remove(tagjob->filename+raw_tag_ext); // delete file
                        } else {

                        }
                }
            }
            tagqueue_mtx.unlock();
            chan_buf.clear();
            tag_buf.clear();
            //chan_buf.reserve(10000000);
            //tag_buf.reserve (10000000);
        } else {
            // if tagbuf is empty, wait for a few ms to not continuously lock the mutex
            //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    std::cout << "exit process tag loop" << std::endl;
}

/*
 * Test function used while implementing other methods
 */
int test_tag_functions() {
    if (0) {
        std::vector<int> chan;
        std::vector<long long> tags_internal;
        std::vector<double> tags;
        
        read_raw_tags("binarytags", chan, tags_internal);
        //read_text_tags("tags", chan, tags_internal);
        
        tags_to_ns(tags_internal, tags);
        
        printf("Read tags: \n");
        for (int i=0; i<10; ++i) {
            printf("%i \t %lli \t %f\n", chan[i], tags_internal[i], tags[i]);
        }
        printf("...\n");
        for (long unsigned int i=chan.size()-10; i<chan.size(); ++i) {
            printf("%i \t %lli \t %f\n", chan[i], tags_internal[i], tags[i]);
        }
        average_rate(tags);
        write_capnp_tags_compressed("tags", chan, tags_internal);
        write_capnp_tags("tags", chan, tags_internal);
        
        std::vector<int> chan_read;
        std::vector<long long> tags_internal_read;
        read_capnp_tags("tags", chan_read, tags_internal_read);
        
        printf("Read tags fron capnproto (uncompressed): \n");
        for (int i=0; i<10; ++i) {
            printf("%i \t %lli\n", chan_read[i], tags_internal_read[i]);
        }
        printf("...\n");
        for (long unsigned int i=chan.size()-10; i<chan.size(); ++i) {
            printf("%i \t %lli\n", chan_read[i], tags_internal_read[i]);
        }
        
        std::vector<int> chan_read_comp;
        std::vector<long long> tags_internal_read_comp;
        read_capnp_tags_compressed("tags", chan_read_comp, tags_internal_read_comp);
        
        printf("Read tags fron capnproto (compressed): \n");
        for (int i=0; i<10; ++i) {
            printf("%i \t %lli\n", chan_read_comp[i], tags_internal_read_comp[i]);
        }
        printf("...\n");
        for (long unsigned int i=chan.size()-10; i<chan.size(); ++i) {
            printf("%i \t %lli\n", chan_read_comp[i], tags_internal_read_comp[i]);
        }
    } else {
        std::thread tagger_thread(send_tags_over_net);
        capnp::EzRpcServer server(kj::heap<TaggerImpl>(), "10.42.0.13:37397");
        std::cout << "Listening" << std::endl;
        kj::NEVER_DONE.wait(server.getWaitScope());
    }
    
    return 0;
}

