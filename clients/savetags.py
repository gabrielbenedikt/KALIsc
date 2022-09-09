#!/usr/bin/env python3

import argparse
import asyncio
import capnp
import cp_tags_capnp
import socket
import numpy as np
import random
import time

async def myreader(client, reader):
    while True:
        data = await reader.read(4096)
        client.write(data)

async def mywriter(client, writer):
    while True:
        data = await client.read(4096)
        writer.write(data.tobytes())
        await writer.drain()

async def request_tags(client, fn, chans, duration):
    tagmsg = client.bootstrap().cast_as(cp_tags_capnp.Tagger)
    tagrequest = tagmsg.savetags_request()
    tagrequest.filename = fn #filename
    tagrequest.chans = chans #channels to save
    tagrequest.duration = duration #seconds
    tagpromise = tagrequest.send()
    tagresult = await tagpromise.a_wait()
    
    job_id = np.array(tagresult.jobid)
    
    print("got jobid", job_id)
    
async def main(host):
    ip = "10.42.0.13"
    port = "37397"

    print("Try IPv4")
    reader, writer = await asyncio.open_connection(
        ip, port,
        family=socket.AF_INET
    )

    client = capnp.TwoPartyClient()
    coroutines = [myreader(client, reader), mywriter(client, writer)]
    asyncio.gather(*coroutines, return_exceptions=True)

    i=1
    while (True):
        await request_tags(client, fn="tags_"+str(i), chans=[8, 12], duration=1)
        time.sleep(3)
        i=i+1

if __name__ == '__main__':
    asyncio.run(main("host"))
