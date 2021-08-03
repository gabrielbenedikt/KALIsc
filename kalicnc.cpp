#include "kalicnc.h"

/*
 * A simple ncurses client
 */
int main() {

    //catch ctrl+c
    std::signal(SIGINT, shutdown);

    init_ui();

    uint64_t jobid;
    uint64_t i=0;
    while (true) {
        jobstruct job;
        jobid = submit_job(0.5);
        while (query_job_done(jobid) == returncodes::NOT_DONE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        get_result(jobid, job);
        mvprintw(19,0, "update # %i", i);
        ++i;
        update_screen(job);
        //print_job(job);
    }

    endwin();

    return 0;
}

void update_screen(jobstruct &job) {
    clear();
    mvprintw(0,0,"singles");
    for (int i = 0; i<16; ++i) {
        mvprintw(i+1, 0, "chn %i: ", i+1);
        mvprintw(i+1, 8, "%.2f", job.events[i]/job.duration_s);
    }

    mvprintw(18, 0, "duration: %.2f s", job.duration_s);
    refresh();
}

void shutdown(int signum) {
    endwin();
    exit(signum);
}

void init_ui() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
}

int get_result(uint64_t id, jobstruct &job) {
    capnp::EzRpcClient client(ip, port);
    Tagger::Client tagger = client.getMain<Tagger>();
    auto request = tagger.getresultsRequest();
    request.setJobid(id);

    auto response = request.send().wait(client.getWaitScope());
    auto payload = response.getPayload();

    std::string err = payload.getErr();
    if (err != std::string("")) {
        //std::cout << err << std::endl;
        return -1;
    } else {
        job.duration_s = payload.getDuration();
        job.finished = payload.getFinished();
        job.id = payload.getId();
        job.start_tag = payload.getStarttag();
        job.stop_tag = payload.getStoptag();
        job.window = payload.getWindow();
        for (uint16_t pat : payload.getPatterns()) {
            job.patterns.push_back(pat);
        }
        for (uint64_t evt : payload.getEvents()) {
            job.events.push_back(evt);
        }
        return 0;
    }
}

uint64_t submit_job(double duration) {
    capnp::EzRpcClient client(ip, port);
    Tagger::Client tagger = client.getMain<Tagger>();
    auto request = tagger.submitjobRequest();

    auto jobdef = request.getJob();
    jobdef.setDuration(duration);
    jobdef.setWindow(120);
    auto pats = jobdef.initPatterns(16);
    pats.set(0,  helpers::channels_to_bitmask(std::vector<uint8_t>{1}));
    pats.set(1,  helpers::channels_to_bitmask(std::vector<uint8_t>{2}));
    pats.set(2,  helpers::channels_to_bitmask(std::vector<uint8_t>{3}));
    pats.set(3,  helpers::channels_to_bitmask(std::vector<uint8_t>{4}));
    pats.set(4,  helpers::channels_to_bitmask(std::vector<uint8_t>{5}));
    pats.set(5,  helpers::channels_to_bitmask(std::vector<uint8_t>{6}));
    pats.set(6,  helpers::channels_to_bitmask(std::vector<uint8_t>{7}));
    pats.set(7,  helpers::channels_to_bitmask(std::vector<uint8_t>{8}));
    pats.set(8,  helpers::channels_to_bitmask(std::vector<uint8_t>{9}));
    pats.set(9,  helpers::channels_to_bitmask(std::vector<uint8_t>{10}));
    pats.set(10, helpers::channels_to_bitmask(std::vector<uint8_t>{11}));
    pats.set(11, helpers::channels_to_bitmask(std::vector<uint8_t>{12}));
    pats.set(12, helpers::channels_to_bitmask(std::vector<uint8_t>{13}));
    pats.set(13, helpers::channels_to_bitmask(std::vector<uint8_t>{14}));
    pats.set(14, helpers::channels_to_bitmask(std::vector<uint8_t>{15}));
    pats.set(15, helpers::channels_to_bitmask(std::vector<uint8_t>{16}));

    auto response = request.send().wait(client.getWaitScope());
    uint64_t jobid = response.getJobid();

    return jobid;
}

int8_t query_job_done(uint64_t id) {
    capnp::EzRpcClient client(ip, port);
    Tagger::Client tagger = client.getMain<Tagger>();
    auto request = tagger.queryjobdoneRequest();
    request.setJobid(id);
    auto response = request.send().wait(client.getWaitScope());

    return response.getRet();
}

void print_job(jobstruct &job) {
    std::cout << "Job\n";
    std::cout << "id: \t" << job.id << "\n";
    for (size_t i=0; i < job.patterns.size(); ++i) {
        std::cout << "pat: \t" << std::bitset<16>(job.patterns[i]) << "\t#: " << job.events[i] << "\n";
    }
    std::cout << "wnd: \t" << job.window << "\n";
    std::cout << "duration: \t" << job.duration << "\n";
    std::cout << "duration: \t" << job.duration_s << "s\n";
    std::cout << "start tag: \t" << job.start_tag << "\n";
    std::cout << "stop tag: \t" << job.stop_tag << "\n";
    std::cout << "finished: \t" << job.finished << "\n";
    std::cout << std::endl;
}
