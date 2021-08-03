@0xd93f59c43d195eec;

using import "ttdata.capnp".TTdata;

struct Job {
    id          @0 : UInt64         =0;     # id                                                                    #only in result
    patterns    @1 : List(UInt16);          # List of bitmasks of channels in your desired coincidence pattern
    events      @2 : List(UInt64);          # List of total events in these patterns                                #only in result
    window      @3 : UInt16;                # your coincidence window in ns
    duration    @4 : Float64;               # Duration during which thise events occurred seconds
    finished    @5 : Bool           =false; #                                                                       #only in result
    starttag    @6 : UInt64;                #                                                                       #only in result
    stoptag     @7 : UInt64;                #                                                                       #only in result
    err         @8 : Text           = "";   #                                                                       #only in result
}

interface Tagger {
    savetags        @0 (filename :Text, chans :List(UInt8),         # to a specified filename
                        duration :UInt16)               -> (jobid :UInt64);     # for duration number of seconds

    submitjob       @1 (job: Job)                       -> (jobid :UInt64);

    queryjobdone    @2 (jobid :UInt64)                  -> (ret :Int8);

    getresults      @3 (jobid :UInt64)                  -> (payload :Job);

    setthreshold    @4 (chan: UInt8, voltage: Float64)  -> (ret: Int8);

    setdelay        @5 (chan: UInt8, delay: Float64)    -> (ret: Int8);
}

