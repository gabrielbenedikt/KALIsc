# addressbook_fast.pyx
# distutils: language = c++
# distutils: include_dirs = /usr/local/lib/python3.9/dist-packages
# distutils: libraries = capnpc capnp capnp-rpc
# distutils: sources = cp_tags.capnp.cpp
# cython: c_string_type = str
# cython: c_string_encoding = default
# cython: embedsignature = True


# TODO: add struct/enum/list types







import capnp
import cp_tags_capnp

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


cdef extern from "cp_tags.capnp.h":
    Schema getJobSchema"capnp::Schema::from<Job>"()

    cdef cppclass Job"Job":
        cppclass Reader:
            uint64_t getId() except +reraise_kj_exception
                
            DynamicValue.Reader getPatterns() except +reraise_kj_exception
            DynamicValue.Reader getEvents() except +reraise_kj_exception
            uint64_t getWindow() except +reraise_kj_exception
                
            DynamicValue.Reader getDuration() except +reraise_kj_exception
            cbool getFinished() except +reraise_kj_exception
                
            uint64_t getStarttag() except +reraise_kj_exception
                
            uint64_t getStoptag() except +reraise_kj_exception
                
            StringPtr getErr() except +reraise_kj_exception
                
        cppclass Builder:
            uint64_t getId() except +reraise_kj_exception
                
            void setId(uint64_t) except +reraise_kj_exception
                
            DynamicValue.Builder getPatterns() except +reraise_kj_exception
            void setPatterns(DynamicValue.Reader) except +reraise_kj_exception
            DynamicValue.Builder getEvents() except +reraise_kj_exception
            void setEvents(DynamicValue.Reader) except +reraise_kj_exception
            uint64_t getWindow() except +reraise_kj_exception
                
            void setWindow(uint16_t) except +reraise_kj_exception
                
            DynamicValue.Builder getDuration() except +reraise_kj_exception
            void setDuration(DynamicValue.Reader) except +reraise_kj_exception
            cbool getFinished() except +reraise_kj_exception
                
            void setFinished(cbool) except +reraise_kj_exception
                
            uint64_t getStarttag() except +reraise_kj_exception
                
            void setStarttag(uint64_t) except +reraise_kj_exception
                
            uint64_t getStoptag() except +reraise_kj_exception
                
            void setStoptag(uint64_t) except +reraise_kj_exception
                
            StringPtr getErr() except +reraise_kj_exception
                
            void setErr(StringPtr) except +reraise_kj_exception
                

    cdef cppclass C_DynamicStruct_Reader" ::capnp::DynamicStruct::Reader":
        Job.Reader asJob"as<Job>"()

    cdef cppclass C_DynamicStruct_Builder" ::capnp::DynamicStruct::Builder":
        Job.Builder asJob"as<Job>"()

_Job_Schema = _Schema()._init(getJobSchema()).as_struct()
cp_tags_capnp.Job.schema = _Job_Schema

cdef class Job_Reader(_DynamicStructReader):
    cdef Job.Reader thisptr_child
    def __init__(self, _DynamicStructReader struct):
        self._init(struct.thisptr, struct._parent, struct.is_root, False)
        self.thisptr_child = (<C_DynamicStruct_Reader>struct.thisptr).asJob()
    

    cpdef _get_id(self):
        return self.thisptr_child.getId()
        

    property id:
        def __get__(self):
            return self._get_id()

    cpdef _get_patterns(self):
        cdef DynamicValue.Reader temp = self.thisptr_child.getPatterns()
        return to_python_reader(temp, self._parent)
        

    property patterns:
        def __get__(self):
            return self._get_patterns()

    cpdef _get_events(self):
        cdef DynamicValue.Reader temp = self.thisptr_child.getEvents()
        return to_python_reader(temp, self._parent)
        

    property events:
        def __get__(self):
            return self._get_events()

    cpdef _get_window(self):
        return self.thisptr_child.getWindow()
        

    property window:
        def __get__(self):
            return self._get_window()

    cpdef _get_duration(self):
        cdef DynamicValue.Reader temp = self.thisptr_child.getDuration()
        return to_python_reader(temp, self._parent)
        

    property duration:
        def __get__(self):
            return self._get_duration()

    cpdef _get_finished(self):
        return self.thisptr_child.getFinished()
        

    property finished:
        def __get__(self):
            return self._get_finished()

    cpdef _get_starttag(self):
        return self.thisptr_child.getStarttag()
        

    property starttag:
        def __get__(self):
            return self._get_starttag()

    cpdef _get_stoptag(self):
        return self.thisptr_child.getStoptag()
        

    property stoptag:
        def __get__(self):
            return self._get_stoptag()

    cpdef _get_err(self):
        temp = self.thisptr_child.getErr()
        return (<char*>temp.begin())[:temp.size()]
        

    property err:
        def __get__(self):
            return self._get_err()

    def to_dict(self, verbose=False, ordered=False):
        ret = {
        
        
        'id': _to_dict(self.id, verbose, ordered),
        
        
        'patterns': _to_dict(self.patterns, verbose, ordered),
        
        
        'events': _to_dict(self.events, verbose, ordered),
        
        
        'window': _to_dict(self.window, verbose, ordered),
        
        
        'duration': _to_dict(self.duration, verbose, ordered),
        
        
        'finished': _to_dict(self.finished, verbose, ordered),
        
        
        'starttag': _to_dict(self.starttag, verbose, ordered),
        
        
        'stoptag': _to_dict(self.stoptag, verbose, ordered),
        
        
        'err': _to_dict(self.err, verbose, ordered),
        
        }

        

        return ret

cdef class Job_Builder(_DynamicStructBuilder):
    cdef Job.Builder thisptr_child
    def __init__(self, _DynamicStructBuilder struct):
        self._init(struct.thisptr, struct._parent, struct.is_root, False)
        self.thisptr_child = (<C_DynamicStruct_Builder>struct.thisptr).asJob()
    
    cpdef _get_id(self):
        return self.thisptr_child.getId()
        
    cpdef _set_id(self, uint64_t value):
        self.thisptr_child.setId(value)
        

    property id:
        def __get__(self):
            return self._get_id()
        def __set__(self, value):
            self._set_id(value)
    cpdef _get_patterns(self):
        cdef DynamicValue.Builder temp = self.thisptr_child.getPatterns()
        return to_python_builder(temp, self._parent)
        
    cpdef _set_patterns(self, list value):
        cdef uint i = 0
        self.init("patterns", len(value))
        cdef _DynamicListBuilder temp =  self._get_patterns()
        for elem in value:
            temp[i] = elem
            i += 1
        

    property patterns:
        def __get__(self):
            return self._get_patterns()
        def __set__(self, value):
            self._set_patterns(value)
    cpdef _get_events(self):
        cdef DynamicValue.Builder temp = self.thisptr_child.getEvents()
        return to_python_builder(temp, self._parent)
        
    cpdef _set_events(self, list value):
        cdef uint i = 0
        self.init("events", len(value))
        cdef _DynamicListBuilder temp =  self._get_events()
        for elem in value:
            temp[i] = elem
            i += 1
        

    property events:
        def __get__(self):
            return self._get_events()
        def __set__(self, value):
            self._set_events(value)
    cpdef _get_window(self):
        return self.thisptr_child.getWindow()
        
    cpdef _set_window(self, uint16_t value):
        self.thisptr_child.setWindow(value)
        

    property window:
        def __get__(self):
            return self._get_window()
        def __set__(self, value):
            self._set_window(value)
    cpdef _get_duration(self):
        cdef DynamicValue.Builder temp = self.thisptr_child.getDuration()
        return to_python_builder(temp, self._parent)
        
    cpdef _set_duration(self, value):
        _setDynamicFieldStatic(self.thisptr, "duration", value, self._parent)
        

    property duration:
        def __get__(self):
            return self._get_duration()
        def __set__(self, value):
            self._set_duration(value)
    cpdef _get_finished(self):
        return self.thisptr_child.getFinished()
        
    cpdef _set_finished(self, cbool value):
        self.thisptr_child.setFinished(value)
        

    property finished:
        def __get__(self):
            return self._get_finished()
        def __set__(self, value):
            self._set_finished(value)
    cpdef _get_starttag(self):
        return self.thisptr_child.getStarttag()
        
    cpdef _set_starttag(self, uint64_t value):
        self.thisptr_child.setStarttag(value)
        

    property starttag:
        def __get__(self):
            return self._get_starttag()
        def __set__(self, value):
            self._set_starttag(value)
    cpdef _get_stoptag(self):
        return self.thisptr_child.getStoptag()
        
    cpdef _set_stoptag(self, uint64_t value):
        self.thisptr_child.setStoptag(value)
        

    property stoptag:
        def __get__(self):
            return self._get_stoptag()
        def __set__(self, value):
            self._set_stoptag(value)
    cpdef _get_err(self):
        temp = self.thisptr_child.getErr()
        return (<char*>temp.begin())[:temp.size()]
        
    cpdef _set_err(self, value):
        cdef StringPtr temp_string
        if type(value) is bytes:
            temp_string = StringPtr(<char*>value, len(value))
        else:
            encoded_value = value.encode('utf-8')
            temp_string = StringPtr(<char*>encoded_value, len(encoded_value))
        self.thisptr_child.setErr(temp_string)
        

    property err:
        def __get__(self):
            return self._get_err()
        def __set__(self, value):
            self._set_err(value)

    def to_dict(self, verbose=False, ordered=False):
        ret = {
        
        
        'id': _to_dict(self.id, verbose, ordered),
        
        
        'patterns': _to_dict(self.patterns, verbose, ordered),
        
        
        'events': _to_dict(self.events, verbose, ordered),
        
        
        'window': _to_dict(self.window, verbose, ordered),
        
        
        'duration': _to_dict(self.duration, verbose, ordered),
        
        
        'finished': _to_dict(self.finished, verbose, ordered),
        
        
        'starttag': _to_dict(self.starttag, verbose, ordered),
        
        
        'stoptag': _to_dict(self.stoptag, verbose, ordered),
        
        
        'err': _to_dict(self.err, verbose, ordered),
        
        }

        

        return ret

    def from_dict(self, dict d):
        cdef str key
        for key, val in d.iteritems():
            if False: pass
        
            elif key == "id":
                try:
                    self._set_id(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_id(val)
                    else:
                        raise
            elif key == "patterns":
                try:
                    self._set_patterns(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_patterns(val)
                    else:
                        raise
            elif key == "events":
                try:
                    self._set_events(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_events(val)
                    else:
                        raise
            elif key == "window":
                try:
                    self._set_window(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_window(val)
                    else:
                        raise
            elif key == "duration":
                try:
                    self._set_duration(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_duration(val)
                    else:
                        raise
            elif key == "finished":
                try:
                    self._set_finished(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_finished(val)
                    else:
                        raise
            elif key == "starttag":
                try:
                    self._set_starttag(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_starttag(val)
                    else:
                        raise
            elif key == "stoptag":
                try:
                    self._set_stoptag(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_stoptag(val)
                    else:
                        raise
            elif key == "err":
                try:
                    self._set_err(val)
                except Exception as e:
                    if 'expected isSetInUnion(field)' in str(e):
                        self.init(key)
                        self._set_err(val)
                    else:
                        raise
            else:
                raise ValueError('Key not found in struct: ' + key)


capnp.register_type(10252874616994876655, (Job_Reader, Job_Builder))
