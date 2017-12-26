#ifndef CTR_CTYPES_H
#define CTR_CTYPES_H

#define CTR_CT_SIMPLE_TYPE_FUNC_MAKE(type) ctr_object* CtrStdCType_##type;\
ctr_object* ctr_ctypes_make_##type (ctr_object* myself, ctr_argument* argumentList)
#define CTR_CT_SIMPLE_TYPE_FUNC_UNMAKE(type) ctr_object* ctr_ctypes_unmake_##type (ctr_object* myself, ctr_argument* argumentList)

#define CTR_CT_SIMPLE_TYPE_FUNC_STR(type)  ctr_object* ctr_ctypes_str_##type (ctr_object* myself, ctr_argument* argumentList)
#define CTR_CT_SIMPLE_TYPE_FUNC_SET(type)  ctr_object* ctr_ctypes_set_##type (ctr_object* myself, ctr_argument* argumentList)
#define CTR_CT_SIMPLE_TYPE_FUNC_GET(type)  ctr_object* ctr_ctypes_get_##type (ctr_object* myself, ctr_argument* argumentList)
#define CTR_CT_FFI_BIND(name)              ctr_object* ctr_ctype_ffi_##name  (ctr_object* myself, ctr_argument* argumentList)

#define CTR_CT_INTRODUCE_TYPE(type)        CtrStdCType_##type = ctr_internal_create_object(CTR_OBJECT_TYPE_OTEX);\
CtrStdCType_##type->link = CtrStdCType;\
CtrStdCType_##type->info.sticky = 1;\
ctr_internal_object_add_property(CtrStdCType, ctr_build_string_from_cstring("type_"#type), CtrStdCType_##type, 0);
#define CTR_CT_INTRODUCE_MAKE(type)        ctr_internal_create_func(CtrStdCType, ctr_build_string_from_cstring(#type), &ctr_ctypes_make_##type)
#define CTR_CT_INTRODUCE_UNMAKE(type)    ctr_internal_create_func(CtrStdCType_##type, ctr_build_string_from_cstring("destruct"), &ctr_ctypes_unmake_##type)
#define CTR_CT_INTRODUCE_SET(type)         ctr_internal_create_func(CtrStdCType_##type, ctr_build_string_from_cstring("set:"), &ctr_ctypes_set_##type)
#define CTR_CT_INTRODUCE_GET(type)         ctr_internal_create_func(CtrStdCType_##type, ctr_build_string_from_cstring("get"), &ctr_ctypes_get_##type)
#include <ffi.h>
#include "../../citron.h"
#include "_struct.h"
#include "structmember.h"

//Unified struct to treat all ffi resources as one Citron resource.
//And also easy allocation
struct ctr_ffi_cfi {
  ffi_cif*   cif;
  ffi_type*  rtype;
  ffi_type** atypes;
};
typedef struct ctr_ffi_cfi ctr_ffi_cfi;

enum ctr_ctype {
  CTR_CTYPE_INVALID = -1, // Error'd
  CTR_CTYPE_VOID = 0,
  CTR_CTYPE_UINT8,
  CTR_CTYPE_SINT8,
  CTR_CTYPE_UINT16,
  CTR_CTYPE_SINT16,
  CTR_CTYPE_UINT32,
  CTR_CTYPE_SINT32,
  CTR_CTYPE_UINT64,
  CTR_CTYPE_SINT64,
  CTR_CTYPE_FLOAT,
  CTR_CTYPE_DOUBLE,
  CTR_CTYPE_UCHAR,
  CTR_CTYPE_SCHAR,
  CTR_CTYPE_USHORT,
  CTR_CTYPE_SSHORT,
  CTR_CTYPE_UINT,
  CTR_CTYPE_SINT,
  CTR_CTYPE_ULONG,
  CTR_CTYPE_SLONG,
  CTR_CTYPE_LONGDOUBLE,
  CTR_CTYPE_POINTER,
  CTR_CTYPE_CIF,
  CTR_CTYPE_DYN_LIB,
  CTR_CTYPE_STRUCT,
  //convenience
  CTR_CTYPE_STRING,
  CTR_CTYPE_FUNCTION_POINTER
};
typedef enum ctr_ctype ctr_ctype;

typedef struct {
    int member_count;
    size_t size;
    ffi_type* type;
    pad_info_node_t* padinfo;
    void* value;
} ctr_ctypes_ffi_struct_value;

ctr_object* CtrStdCType; //Template, not added to the world
ctr_object* CtrStdCType_ffi_cif;

//Void
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(void);
CTR_CT_SIMPLE_TYPE_FUNC_SET(void);
CTR_CT_SIMPLE_TYPE_FUNC_GET(void);

//Unsigned Int 8
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(uint8);
CTR_CT_SIMPLE_TYPE_FUNC_SET(uint8);
CTR_CT_SIMPLE_TYPE_FUNC_GET(uint8);

//Signed Int 8
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(sint8);
CTR_CT_SIMPLE_TYPE_FUNC_SET(sint8);
CTR_CT_SIMPLE_TYPE_FUNC_GET(sint8);

//Unsigned Int 16
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(uint16);
CTR_CT_SIMPLE_TYPE_FUNC_SET(uint16);
CTR_CT_SIMPLE_TYPE_FUNC_GET(uint16);

//Signed Int 16
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(sint16);
CTR_CT_SIMPLE_TYPE_FUNC_SET(sint16);
CTR_CT_SIMPLE_TYPE_FUNC_GET(sint16);

//Unsigned Int 32
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(uint32);
CTR_CT_SIMPLE_TYPE_FUNC_SET(uint32);
CTR_CT_SIMPLE_TYPE_FUNC_GET(uint32);

//Signed Int 32
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(sint32);
CTR_CT_SIMPLE_TYPE_FUNC_SET(sint32);
CTR_CT_SIMPLE_TYPE_FUNC_GET(sint32);

//Unsigned Int 64
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(uint64);
CTR_CT_SIMPLE_TYPE_FUNC_SET(uint64);
CTR_CT_SIMPLE_TYPE_FUNC_GET(uint64);

//Signed Int 64
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(sint64);
CTR_CT_SIMPLE_TYPE_FUNC_SET(sint64);
CTR_CT_SIMPLE_TYPE_FUNC_GET(sint64);

//Float
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(float);
CTR_CT_SIMPLE_TYPE_FUNC_SET(float);
CTR_CT_SIMPLE_TYPE_FUNC_GET(float);

//Double
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(double);
CTR_CT_SIMPLE_TYPE_FUNC_SET(double);
CTR_CT_SIMPLE_TYPE_FUNC_GET(double);

//Unsigned Char
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(uchar);
CTR_CT_SIMPLE_TYPE_FUNC_SET(uchar);
CTR_CT_SIMPLE_TYPE_FUNC_GET(uchar);

//Signed Char
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(schar);
CTR_CT_SIMPLE_TYPE_FUNC_SET(schar);
CTR_CT_SIMPLE_TYPE_FUNC_GET(schar);

//Unsigned Short
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(ushort);
CTR_CT_SIMPLE_TYPE_FUNC_SET(ushort);
CTR_CT_SIMPLE_TYPE_FUNC_GET(ushort);

//Signed Short
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(sshort);
CTR_CT_SIMPLE_TYPE_FUNC_SET(sshort);
CTR_CT_SIMPLE_TYPE_FUNC_GET(sshort);

//Unsigned Int
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(uint);
CTR_CT_SIMPLE_TYPE_FUNC_SET(uint);
CTR_CT_SIMPLE_TYPE_FUNC_GET(uint);

//Signed Int
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(sint);
CTR_CT_SIMPLE_TYPE_FUNC_SET(sint);
CTR_CT_SIMPLE_TYPE_FUNC_GET(sint);

//Unsigned Long
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(ulong);
CTR_CT_SIMPLE_TYPE_FUNC_SET(ulong);
CTR_CT_SIMPLE_TYPE_FUNC_GET(ulong);

//Signed Long
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(slong);
CTR_CT_SIMPLE_TYPE_FUNC_SET(slong);
CTR_CT_SIMPLE_TYPE_FUNC_GET(slong);

//Long Double
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(longdouble);
CTR_CT_SIMPLE_TYPE_FUNC_SET(longdouble);
CTR_CT_SIMPLE_TYPE_FUNC_GET(longdouble);

//Pointer
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(pointer);
CTR_CT_SIMPLE_TYPE_FUNC_SET(pointer);
CTR_CT_SIMPLE_TYPE_FUNC_STR(pointer);

//Dynamic Library
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(dynamic_lib);
CTR_CT_SIMPLE_TYPE_FUNC_GET(dynamic_lib);
CTR_CT_SIMPLE_TYPE_FUNC_STR(dynamic_lib);

//Dynamic Library
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(struct);

//String
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(string);
CTR_CT_SIMPLE_TYPE_FUNC_SET(string);
CTR_CT_SIMPLE_TYPE_FUNC_STR(string);

//Function
CTR_CT_SIMPLE_TYPE_FUNC_MAKE(functionptr);
CTR_CT_SIMPLE_TYPE_FUNC_SET(functionptr);
CTR_CT_SIMPLE_TYPE_FUNC_STR(functionptr);

//FFI bindings
CTR_CT_FFI_BIND(prep_cif);
CTR_CT_FFI_BIND(cif_new);
CTR_CT_FFI_BIND(call);

ffi_type* ctr_ctypes_ffi_convert_to_ffi_type(ctr_object* type);
ctr_object* ctr_ctypes_get_first_meta(ctr_object* object, ctr_object* last);
#endif
