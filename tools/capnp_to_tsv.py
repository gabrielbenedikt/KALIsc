#!/usr/bin/env python3
import capnp
import cp_tags_capnp
import numpy as np
import pandas as pd
import ttdata_capnp
import zstandard

zstdfn = "tags_2.cpdat.zstd"
capnpfn = zstdfn.replace("zstd","cpnp")
tsvfn = zstdfn.replace("zstd","tsv")

with open(zstdfn, 'rb') as zstdfile:
    decompressor = zstandard.ZstdDecompressor()
    with open(capnpfn, 'wb') as capnpfile:
        decompressor.copy_stream(zstdfile, capnpfile)

with open(capnpfn, 'rb') as capnpf:
    capdata = ttdata_capnp.TTdata.read(capnpf)

channels = np.array(capdata.chan[0])
tags = np.array(capdata.tag[0])

print(channels)
print(tags)

df = pd.DataFrame(columns=['chn','tag'])
df.chn = channels
df.tag = tags

print(len(df.chn))
print(len(df.tag))
df.to_csv(tsvfn, sep='\t')
