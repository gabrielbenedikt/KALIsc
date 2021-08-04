# addressbook_fast.pyx
# distutils: language = c++
# distutils: include_dirs = /usr/local/lib/python3.9/dist-packages
# distutils: libraries = capnpc capnp capnp-rpc
# distutils: sources = ttdata.capnp.cpp
# cython: c_string_type = str
# cython: c_string_encoding = default
# cython: embedsignature = True


# TODO: add struct/enum/list types







import capnp
import ttdata_capnp

from capnp.includes.types cimport *
from capnp cimport helpers
from capnp.includes.capnp_cpp cimport DynamicValue, Schema, VOID, StringPtr, ArrayPtr, Data
from capnp.lib.capnp cimport _DynamicStructReader, _DynamicStructBuilder, _DynamicListBuilder, _DynamicEnum, _StructSchemaField, to_python_builder, to_python_reader, _to_dict, _setDynamicFieldStatic, _Schema, _InterfaceSchema

from capnp.helpers.non_circular cimport reraise_kj_exception

cdef DynamicValue.Reader _extract_dynamic_struct_builder(_DynamicStructBuilder value):
    return DynamicValue.Reader(value.thisptr.asReader())

cdef DynamicValue.Reader _extract_dynamic_struct_reader(_DynamicStructReader value):
    return DynamicValue.Reader(value.thisptr)

cdef DynamicValue.Reader _extract_dynamic_enum(_DynamicEnum value):
    return DynamicValue.Reader(value.thisptr)

cdef _from_list(_DynamicListBuilder msg, list d):
    cdef size_t count = 0
    for val in d:
        msg._set(count, val)
        count += 1


cdef extern from "ttdata.capnp.h":
    Schema getTTdataSchema"capnp::Schema::from<TTdata>"()

    cdef cppclass TTdata"TTdata":
        cppclass Reader:
            DynamicValue.Reader getChan() except +reraise_kj_exception
            DynamicValue.Reader getTag() except +reraise_kj_exception
        cppclass Builder:
            DynamicValue.Builder getChan() except +reraise_kj_exception
            void setChan(DynamicValue.Reader) except +reraise_kj_exception
            DynamicValue.Builder getTag() except +reraise_kj_exception
            void setTag(DynamicValue.Reader) except +reraise_kj_exception

    cdef cppclass C_DynamicStruct_Reader" ::capnp::DynamicStruct::Reader":
        TTdata.Reader asTTdata"as<TTdata>"()

    cdef cppclass C_DynamicStruct_Builder" ::capnp::DynamicStruct::Builder":
        TTdata.Builder asTTdata"as<TTdata>"()

_TTdata_Schema = _Schema()._init(getTTdataSchema()).as_struct()
ttdata_capnp.TTdata.schema = _TTdata_Schema

cdef class TTdata_Reader(_DynamicStructReader):
    cdef TTdata.Reader thisptr_child
    def __init__(self, _DynamicStructReader struct):
        self._init(struct.thisptr, struct._parent, struct.is_root, False)
        self.thisptr_child = (<C_DynamicStruct_Reader>struct.thisptr).asTTdata()
    

    cpdef _get_chan(self):
        cdef DynamicValue.Reader temp = self.thisptr_child.getChan()
        return to_python_reader(temp, self._parent)
        

    property chan:
        def __get__(self):
            return self._get_chan()

    cpdef _get_tag(self):
        cdef DynamicValue.Reader temp = self.thisptr_child.getTag()
        return to_python_reader(temp, self._parent)
        

    property tag:
        def __get__(self):
            return self._get_tag()

    def to_dict(self, verbose=False, ordered=False):
        ret = {
        
        
        'chan': _to_dict(self.chan, verbose, ordered),
        
        
        'tag': _to_dict(self.tag, verbose, ordered),
        
        }

        

        return ret

cdef class TTdata_Builder(_DynamicStructBuilder):
    cdef TTdata.Builder thisptr_child
    def __init__(self, _DynamicStructBuilder struct):
        self._init(struct.thisptr, struct._parent, struct.is_root, False)
        self.thisptr_child = (<C_DynamicStruct_Builder>struct.thisptr).asTTdata()
    
    cpdef _get_chan(self):
        cdef DynamicValue.Builder temp = self.thisptr_child.getChan()
        return to_python_builder(temp, self._parent)
        
    cpdef _set_chan(self, list value):
        cdef uint i = 0
        self.init("chan", len(value))
        cdef _DynamicListBuilder temp =  self._get_chan()
        for elem in value:
            temp[i] = elem
            i += 1
        

    property chan:
        def __get__(self):
            return self._get_chan()
        def __set__(self, value):
            self._set_chan(value)
    cpdef _get_tag(self):
        cdef DynamicValue.Builder temp = self.thisptr_child.getTag()
        return to_python_builder(temp, self._parent)
        
    cpdef _set_tag(self, list value):
        cdef uint i = 0
        self.init("tag", len(value))
        cdef _DynamicListBuilder temp =  self._get_tag()
        for elem in value:
            temp[i] = elem
            i += 1
        

    property tag:
        def __get__(self):
            return self._get_tag()
        def __set__(self, value):
            self._set_tag(value)

    def to_dict(self, verbose=False, ordered=False):
        ret = {
        
        
        'chan': _to_dict(self.chan, verbose, ordered),
        
        
        'tag': _to_dict(self.tag, verbose, ordered),
        
        }

        

        return ret

    def from_dict(self, dict d):
        cdef str key
        for key, val in d.iteritems():
            if False: pass
        
            elif key == "chan":
                try:
                    self._set_chan(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_chan(val)
                    else:
                        raise
            elif key == "tag":
                try:
                    self._set_tag(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_tag(val)
                    else:
                        raise
            else:
                raise ValueError('Key not found in struct: ' + key)


capnp.register_type(18341955630557649161, (TTdata_Reader, TTdata_Builder))
