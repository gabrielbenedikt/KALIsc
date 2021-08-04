#!/usr/bin/bash
capnp compile -oc++ --src-prefix=.. ../cp_tags.capnp
capnp compile -ocython --src-prefix=.. ../cp_tags.capnp
python3 setup_capnp.py build_ext --inplace
#rm -r build/
#rm cp_tags.capnp.c++ cp_tags.capnp.cpp cp_tags_capnp_cython.cpp cp_tags.capnp.h
