#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "citron.h"
#include "siphash.h"

static const all_signals[] = {
#ifdef SIGHUP
  SIGHUP,
#endif
#ifdef SIGINT
  SIGINT,
#endif
#ifdef SIGQUIT
  SIGQUIT,
#endif
#ifdef SIGILL
  SIGILL,
#endif
#ifdef SIGABRT
  SIGABRT,
#endif
#ifdef SIGFPE
  SIGFPE,
#endif
#ifdef SIGSEGV
  SIGSEGV,
#endif
#ifdef SIGPIPE
  SIGPIPE,
#endif
#ifdef SIGALRM
  SIGALRM,
#endif
#ifdef SIGTERM
  SIGTERM,
#endif
#ifdef SIGUSR1
  SIGUSR1,
#endif
#ifdef SIGUSR2
  SIGUSR2,
#endif
#ifdef SIGCHLD
  SIGCHLD,
#endif
#ifdef SIGCONT
  SIGCONT,
#endif
#ifdef SIGSTP
  SIGSTP,
#endif
#ifdef SIGTTIN
  SIGTTIN,
#endif
#ifdef SIGTTOU
  SIGTTOU,
#endif
#ifdef SIGBUS
  SIGBUS,
#endif
#ifdef SIGPOLL
  SIGPOLL,
#endif
#ifdef SIGPROF
  SIGPROF,
#endif
#ifdef SIGSYS
  SIGSYS,
#endif
#ifdef SIGTRAP
  SIGTRAP,
#endif
#ifdef SIGURG
  SIGURG,
#endif
#ifdef SIGVTALRM
  SIGVTALRM,
#endif
#ifdef SIGXCPU
  SIGXCPU,
#endif
#ifdef SIGXFSZ
  SIGXFSZ,
#endif
#ifdef SIGEMT
  SIGEMT,
#endif
#ifdef SIGSTKFLT
  SIGSTKFLT,
#endif
#ifdef SIGIO
  SIGIO,
#endif
#ifdef SIGPWR
  SIGPWR,
#endif
#ifdef SIGLOST
  SIGLOST,
#endif
#ifdef SIGWINCH
  SIGWINCH,
#endif
  SIGKILL, //last one. must exist
};
static void register_signal_handlers() {
  struct sigaction act;
  int i = 0;
  memset(&act, 0, sizeof act);
  act.sa_handler = ctr_int_handler;
  act.sa_flags = SA_NOCLDSTOP;
  do {
    if(sigaction(all_signals[i], &act, NULL))
      fprintf(stderr, "Could not install signal %d handler: %s (Ignoring)\n", all_signals[i], strerror(errno));
  } while(all_signals[++i] != SIGKILL);
}

/**
 * @internal
 *
 * ReadFile
 *
 * Reads in an entire file.
 */
char* ctr_internal_readf(char* file_name, uint64_t* total_size) {
    char* prg;
    char ch;
    int prev;
    uint64_t size;
    uint64_t real_size;
    FILE* fp;
    fp = fopen(file_name,"r");
    if( fp == NULL ) {
        printf("Error while opening the file (%s).\n", file_name);
        exit(1);
    }
    prev = ftell(fp);
    fseek(fp,0L,SEEK_END);
    size = ftell(fp);
    fseek(fp,prev,SEEK_SET);
    real_size = (size+4)*sizeof(char);
    prg = ctr_heap_allocate(real_size); /* add 4 bytes, 3 for optional closing sequence verbatim mode and one lucky byte! */
    ctr_program_length=0;
    while( ( ch = fgetc(fp) ) != EOF ) prg[ctr_program_length++]=ch;
    fclose(fp);
    *total_size = (uint64_t) real_size;
    return prg;
}

/**
 * @internal
 *
 * InternalObjectIsEqual
 *
 * Detemines whether two objects are identical.
 */
int ctr_internal_object_is_equal(ctr_object* object1, ctr_object* object2) {
    if (object1 == object2) return 1;
    char* string1;
    char* string2;
    ctr_size len1;
    ctr_size len2;
    ctr_size d;
    if (object1->info.type == CTR_OBJECT_TYPE_OTSTRING && object2->info.type == CTR_OBJECT_TYPE_OTSTRING) {
        string1 = object1->value.svalue->value;
        string2 = object2->value.svalue->value;
        len1 = object1->value.svalue->vlen;
        len2 = object2->value.svalue->vlen;
        if (len1 != len2) return 0;
        d = memcmp(string1, string2, len1);
        if (d==0) return 1;
        return 0;
    }
    if (object1->info.type == CTR_OBJECT_TYPE_OTNUMBER && object2->info.type == CTR_OBJECT_TYPE_OTNUMBER) {
        ctr_number num1 = object1->value.nvalue;
        ctr_number num2 = object2->value.nvalue;
        if (num1 == num2) return 1;
        return 0;
    }
    if (object1->info.type == CTR_OBJECT_TYPE_OTBOOL && object2->info.type == CTR_OBJECT_TYPE_OTBOOL) {
        int b1 = object1->value.bvalue;
        int b2 = object2->value.bvalue;
        if (b1 == b2) return 1;
        return 0;
    }
    if (object1->info.type == CTR_OBJECT_TYPE_OTARRAY && object2->info.type == CTR_OBJECT_TYPE_OTARRAY) {
        int count = ctr_array_count(object1, NULL)->value.nvalue;
        if (ctr_array_count(object2, NULL)->value.nvalue != count) return 0;
        int i = 1;
        ctr_argument* args = ctr_heap_allocate(sizeof(ctr_argument));
        for(;count>0&&i;count--) {
          args->object = ctr_build_number_from_float(count-1);
          i = i && ctr_internal_object_is_equal(ctr_array_get(object1,args), ctr_array_get(object2, args));
        }
        ctr_heap_free(args);
        return i;
    }
    if (object1->info.type == CTR_OBJECT_TYPE_OTOBJECT && object2->info.type == CTR_OBJECT_TYPE_OTOBJECT && ctr_reflect_get_primitive_link(object1) == CtrStdMap && ctr_reflect_get_primitive_link(object2) == CtrStdMap) {
      ctr_argument* args = ctr_heap_allocate(sizeof(ctr_argument));
      args->object = object1;
      ctr_object* o1t = ctr_reflect_describe_value(CtrStdReflect, args);
      args->object = object2;
      ctr_object* o2t = ctr_reflect_describe_value(CtrStdReflect, args);
      ctr_heap_free(args);
      return ctr_internal_object_is_equal(o1t, o2t);
    }
    if(ctr_internal_has_responder(object1, ctr_build_string_from_cstring("=")))
    if(ctr_internal_has_responder(object2, ctr_build_string_from_cstring("="))) {
      int a = ctr_internal_cast2bool(ctr_send_message_variadic(object1, "=", 1, 1, object2))->value.bvalue && ctr_internal_cast2bool(ctr_send_message_variadic(object2, "=", 1, 1, object1))->value.bvalue;
      printf("%d\n", a);
      return a;
    }
    return 0;
}
/**
 * @internal
 *
 * InternalObjectIsConstructible
 *
 * Detemines whether an object can be constructed (partly or fully) from another
 */
int ctr_internal_object_is_constructible(ctr_object* object1, ctr_object* object2) {return ctr_internal_object_is_constructible_(object1, object2, 0);}
int ctr_internal_object_is_constructible_(ctr_object* object1, ctr_object* object2, int raw) {
    if (object1 == object2) return 1;
    char* string1;
    char* string2;
    ctr_size len1;
    ctr_size len2;
    ctr_size d;
    if (raw && object2->info.type == CTR_OBJECT_TYPE_OTSTRING) return 1;
    if (object1->info.type == CTR_OBJECT_TYPE_OTSTRING && object2->info.type == CTR_OBJECT_TYPE_OTSTRING) {
        string1 = object1->value.svalue->value;
        string2 = object2->value.svalue->value;
        len1 = object1->value.svalue->vlen;
        len2 = object2->value.svalue->vlen;
        if (len1 != len2) return 0;
        d = memcmp(string1, string2, len1);
        if (d==0) return 1;
        return 0;
    }
    if (object1->info.type == CTR_OBJECT_TYPE_OTNUMBER && object2->info.type == CTR_OBJECT_TYPE_OTNUMBER) {
        ctr_number num1 = object1->value.nvalue;
        ctr_number num2 = object2->value.nvalue;
        if (num1 == num2) return 1;
        return 0;
    }
    if (object1->info.type == CTR_OBJECT_TYPE_OTBOOL && object2->info.type == CTR_OBJECT_TYPE_OTBOOL) {
        int b1 = object1->value.bvalue;
        int b2 = object2->value.bvalue;
        if (b1 == b2) return 1;
        return 0;
    }
    if (object1->info.type == CTR_OBJECT_TYPE_OTARRAY && object2->info.type == CTR_OBJECT_TYPE_OTARRAY) {
        int count = ctr_array_count(object2, NULL)->value.nvalue;
        ctr_object* last = ctr_array_last(object1, NULL);
        if (ctr_array_count(object1, NULL)->value.nvalue < count && !(last->info.type == CTR_OBJECT_TYPE_OTSTRING)) return 0; //It requires more parameters than object1 can provide, and we don't have a catch-all binding
        int i = 1;
        ctr_argument* args = ctr_heap_allocate(sizeof(ctr_argument));
        ctr_argument* elnu = ctr_heap_allocate(sizeof(ctr_argument));
        for(;count>0&&i;count--) {
          elnu->object = ctr_build_number_from_float(i-1);
          args->object = ctr_array_get(object2, elnu);
          i = i && (
            args->object->info.type == CTR_OBJECT_TYPE_OTARRAY && ctr_internal_object_is_constructible_(ctr_array_get(object1, elnu), args->object, raw)
            || args->object->info.type == CTR_OBJECT_TYPE_OTSTRING
            || ctr_array_contains(object1, args)->value.bvalue
          );
        }
        ctr_heap_free(args);
        return i;
    }
    if (object1->info.type == CTR_OBJECT_TYPE_OTOBJECT && object2->info.type == CTR_OBJECT_TYPE_OTOBJECT && ctr_reflect_get_primitive_link(object1) == CtrStdMap && ctr_reflect_get_primitive_link(object2) == CtrStdMap) {
      ctr_argument* args = ctr_heap_allocate(sizeof(ctr_argument));
      args->object = object1;
      ctr_object* o1t = ctr_reflect_describe_value(CtrStdReflect, args);
      args->object = object2;
      ctr_object* o2t = ctr_reflect_describe_value(CtrStdReflect, args);
      ctr_heap_free(args);

      return ctr_internal_object_is_constructible_(o1t, o2t, raw);
    }
    ctr_argument* args = ctr_heap_allocate(sizeof(ctr_argument));
    args->object = object2;
    ctr_object* ret = (ctr_send_message(object1, "isConstructible:", 16, args));
    ctr_heap_free(args);
    return (ret->info.type == CTR_OBJECT_TYPE_OTBOOL) ? ret->value.bvalue : 0;
}

/**
 * @internal
 *
 * InternalObjectIndexHash
 *
 * Given an object, this function will calculate a hash to speed-up
 * lookup.
 */
uint64_t ctr_internal_index_hash(ctr_object* key) {
    ctr_object* stringKey = ctr_internal_cast2string(key);
    return siphash24(stringKey->value.svalue->value, stringKey->value.svalue->vlen, CtrHashKey);
}

/**
 * @internal
 *
 * InternalObjectFindProperty
 *
 * Finds property in object.
 */
ctr_object* ctr_internal_object_find_property(ctr_object* owner, ctr_object* key, int is_method) {
    return ctr_internal_object_find_property_ignore(owner, key, is_method, 0);
}

ctr_object* ctr_internal_object_find_property_ignore(ctr_object* owner, ctr_object* key, int is_method, int ignore) {
    ctr_mapitem* head;
    uint64_t hashKey = ctr_internal_index_hash(key);

#ifdef CTR_DEBUG_HIDING
    int did_skip = -1;
    unsigned t = 0;
    if (!ignore) {
        ctr_object* hide_obj = NULL;
        ctr_object* hide_key = ctr_build_string_from_cstring( "hides" );
        hide_obj = ctr_internal_object_find_property_ignore(owner, hide_key, 0, 1);
        if (hide_obj != NULL && hide_obj->info.type == CTR_OBJECT_TYPE_OTARRAY) {
            ctr_size length = (int) ctr_array_count( hide_obj,  NULL )->value.nvalue;
            int i = 0;
            for ( i = 0; i < length; i ++ ) {
                if((t = ctr_internal_object_is_equal(key, *(hide_obj->value.avalue->elements+i)))) {
                    did_skip = i;
                    //printf("Found object at %d\n",did_skip);
                    break;
                }
            }
        }
    }
#endif //CTR_DEBUG_HIDING

    if (is_method) {
        if (owner->methods->size == 0) {
            return NULL;
        }
        head = owner->methods->head;
    } else {
        if (owner->properties->size == 0) {
            return NULL;
        }
        head = owner->properties->head;
    }
#ifdef CTR_DEBUG_HIDING
    int i = -1;
#endif
    while(head) {
#ifdef CTR_DEBUG_HIDING
        i += 1;
#endif
        if ((hashKey == head->hashKey) && ctr_internal_object_is_equal(head->key, key)) {
#ifdef CTR_DEBUG_HIDING
            if (ignore || i != did_skip)
#endif //CTR_DEBUG_HIDING
                return head->value;
        }
        head = head->next;
    }
    return NULL;
}

ctr_object* ctr_internal_object_find_property_with_hash(ctr_object* owner, ctr_object* key, uint64_t hashKey, int is_method) {
    ctr_mapitem* head;
    if (is_method) {
        if (owner->methods->size == 0) {
            return NULL;
        }
        head = owner->methods->head;
    } else {
        if (owner->properties->size == 0) {
            return NULL;
        }
        head = owner->properties->head;
    }
    while(head) {
        if ((hashKey == head->hashKey)) {
            if(ctr_internal_object_is_equal(head->key, key)) {
                return head->value;
              }
            }
        head = head->next;
    }
    return NULL;
}

/**
 * @internal
 *
 * InternalObjectDeleteProperty
 *
 * Deletes the specified property from the object.
 */
void ctr_internal_object_delete_property(ctr_object* owner, ctr_object* key, int is_method) {
    uint64_t hashKey = ctr_internal_index_hash(key);
    ctr_mapitem* head;
    if (is_method) {
        if (owner->methods->size == 0) {
            return;
        }
        head = owner->methods->head;
    } else {
        if (owner->properties->size == 0) {
            return;
        }
        head = owner->properties->head;
    }
    while(head) {
        if ((hashKey == head->hashKey) && ctr_internal_object_is_equal(head->key, key)) {
            if (head->next && head->prev) {
                head->next->prev = head->prev;
                head->prev->next = head->next;
            } else {
                if (head->next) {
                    head->next->prev = NULL;
                }
                if (head->prev) {
                    head->prev->next = NULL;
                }
            }
            if (is_method) {
                if (owner->methods->head == head) {
                    if (head->next) {
                        owner->methods->head = head->next;
                    } else {
                        owner->methods->head = NULL;
                    }
                }
                owner->methods->size --;
            } else {
                if (owner->properties->head == head) {
                    if (head->next) {
                        owner->properties->head = head->next;
                    } else {
                        owner->properties->head = NULL;
                    }
                }
                owner->properties->size --;
            }
            ctr_heap_free( head );
            return;
        }
        head = head->next;
    }
    return;
}
/**
 * @internal
 *
 * InternalObjectDeletePropertyWithHash
 *
 * Deletes the specified property from the object given a hash value.
 */
void ctr_internal_object_delete_property_with_hash(ctr_object* owner, ctr_object* key, uint64_t hashKey, int is_method) {
    ctr_mapitem* head;
    if (is_method) {
        if (owner->methods->size == 0) {
            return;
        }
        head = owner->methods->head;
    } else {
        if (owner->properties->size == 0) {
            return;
        }
        head = owner->properties->head;
    }
    while(head) {
        if ((hashKey == head->hashKey) && ctr_internal_object_is_equal(head->key, key)) {
            if (head->next && head->prev) {
                head->next->prev = head->prev;
                head->prev->next = head->next;
            } else {
                if (head->next) {
                    head->next->prev = NULL;
                }
                if (head->prev) {
                    head->prev->next = NULL;
                }
            }
            if (is_method) {
                if (owner->methods->head == head) {
                    if (head->next) {
                        owner->methods->head = head->next;
                    } else {
                        owner->methods->head = NULL;
                    }
                }
                owner->methods->size --;
            } else {
                if (owner->properties->head == head) {
                    if (head->next) {
                        owner->properties->head = head->next;
                    } else {
                        owner->properties->head = NULL;
                    }
                }
                owner->properties->size --;
            }
            ctr_heap_free( head );
            return;
        }
        head = head->next;
    }
    return;
}

/**
 * @internal
 *
 * InternalObjectAddProperty
 *
 * Adds a property to an object.
 */
void ctr_internal_object_add_property(ctr_object* owner, ctr_object* key, ctr_object* value, int m) {
    if(strncmp(key->value.svalue->value, "me", key->value.svalue->vlen) != 0)  value->lexical_name = key;
    ctr_mapitem* new_item = ctr_heap_allocate(sizeof(ctr_mapitem));
    ctr_mapitem* current_head = NULL;
    new_item->key = key;
    new_item->hashKey = ctr_internal_index_hash(key);
    new_item->value = value;
    new_item->next = NULL;
    new_item->prev = NULL;
    if (m) {
        if (owner->methods->size == 0) {
            owner->methods->head = new_item;
        } else {
            current_head = owner->methods->head;
            current_head->prev = new_item;
            new_item->next = current_head;
            owner->methods->head = new_item;
        }
        owner->methods->size ++;
    } else {
        if (owner->properties->size == 0) {
            owner->properties->head = new_item;
        } else {
            current_head = owner->properties->head;
            current_head->prev = new_item;
            new_item->next = current_head;
            owner->properties->head = new_item;
        }
        owner->properties->size ++;
    }
}

/**
 * @internal
 *
 * InternalObjectAddPropertyWithHash
 *
 * Adds a property to an object.
 */
void ctr_internal_object_add_property_with_hash(ctr_object* owner, ctr_object* key, uint64_t hashKey, ctr_object* value, int m) {
    ctr_mapitem* new_item = ctr_heap_allocate(sizeof(ctr_mapitem));
    ctr_mapitem* current_head = NULL;
    new_item->key = key;
    new_item->hashKey = hashKey;
    new_item->value = value;
    new_item->next = NULL;
    new_item->prev = NULL;
    if (m) {
        if (owner->methods->size == 0) {
            owner->methods->head = new_item;
        } else {
            current_head = owner->methods->head;
            current_head->prev = new_item;
            new_item->next = current_head;
            owner->methods->head = new_item;
        }
        owner->methods->size ++;
    } else {
        if (owner->properties->size == 0) {
            owner->properties->head = new_item;
        } else {
            current_head = owner->properties->head;
            current_head->prev = new_item;
            new_item->next = current_head;
            owner->properties->head = new_item;
        }
        owner->properties->size ++;
    }
}

/**
 * @internal
 *
 * InternalObjectSetProperty
 *
 * Sets a property on an object.
 */
void ctr_internal_object_set_property(ctr_object* owner, ctr_object* key, ctr_object* value, int is_method) {
    ctr_internal_object_delete_property(owner, key, is_method);
    ctr_internal_object_add_property(owner, key, value, is_method);
}
/**
 * @internal
 *
 * InternalObjectSetPropertyWithHash
 *
 * Sets a property on an object.
 */
void ctr_internal_object_set_property_with_hash(ctr_object* owner, ctr_object* key, uint64_t hashKey, ctr_object* value, int is_method) {
    ctr_internal_object_delete_property_with_hash(owner, key, hashKey, is_method);
    ctr_internal_object_add_property_with_hash(owner, key, hashKey, value, is_method);
}

/**
 * @internal
 *
 * InternalMemMem
 *
 * memmem implementation because this not available on every system.
 */
char* ctr_internal_memmem(char* haystack, long hlen, char* needle, long nlen, int reverse ) {
    char* cur;
    char* last;
    char* begin;
    int step = (1 - reverse * 2); /* 1 if reverse = 0, -1 if reverse = 1 */
    if (nlen == 0) return NULL;
    if (hlen == 0) return NULL;
    if (hlen < nlen) return NULL;
    if (!reverse) {
        begin = haystack;
        last = haystack + hlen - nlen + 1;
    } else {
        begin = haystack + hlen - nlen;
        last = haystack - 1;
    }
    for(cur = begin; cur!=last; cur += step) {
        if (memcmp(cur,needle,nlen) == 0) return cur;
    }
    return NULL;
}

/**
 * @internal
 *
 * InternalObjectCreate
 *
 * Creates an object.
 */
inline ctr_object* ctr_internal_create_object(int type) {
  return ctr_internal_create_mapped_object(type, 0);
}

ctr_object* ctr_internal_create_mapped_object(int type, int shared) {
  ctr_object* o;
  o = shared==1?ctr_heap_allocate_shared(sizeof(ctr_object)):ctr_heap_allocate(sizeof(ctr_object));
  o->properties = shared==1?ctr_heap_allocate_shared(sizeof(ctr_map)):ctr_heap_allocate(sizeof(ctr_map));
  o->methods = shared==1?ctr_heap_allocate_shared(sizeof(ctr_map)):ctr_heap_allocate(sizeof(ctr_map));
  o->properties->size = 0;
  o->methods->size = 0;
  o->properties->head = NULL;
  o->methods->head = NULL;
  o->info.type = type;
  o->info.sticky = 0;
  o->info.mark = 0;
  o->info.remote = 0;
  o->info.shared = shared;
  o->info.raw = 0;
  if (type==CTR_OBJECT_TYPE_OTBOOL) o->value.bvalue = 0;
  if (type==CTR_OBJECT_TYPE_OTNUMBER) o->value.nvalue = 0;
  if (type==CTR_OBJECT_TYPE_OTSTRING) {
      o->value.svalue = shared==1?ctr_heap_allocate_shared(sizeof(ctr_string)):ctr_heap_allocate(sizeof(ctr_string));
      o->value.svalue->value = "";
      o->value.svalue->vlen = 0;
  }
  o->gnext = NULL;
  if (ctr_first_object == NULL) {
      ctr_first_object = o;
  } else {
      o->gnext = ctr_first_object;
      ctr_first_object = o;
  }
  return o;
}
/**
 * @internal
 *
 * InternalFunctionCreate
 *
 * Create a function and add this to the object as a method.
 */
void ctr_internal_create_func(ctr_object* o, ctr_object* key, ctr_object* (*func)( ctr_object*, ctr_argument* ) ) {
    ctr_object* methodObject = ctr_internal_create_object(CTR_OBJECT_TYPE_OTNATFUNC);
    methodObject->value.fvalue = func;
    ctr_internal_object_add_property(o, key, methodObject, 1);
}

/**
 * @internal
 *
 * InternalNumberCast
 *
 * Casts an object to a number object.
 */
 ctr_object* ctr_internal_cast2number(ctr_object* o) {
    if ( o->info.type == CTR_OBJECT_TYPE_OTNUMBER ) return o;
    ctr_argument* a = ctr_heap_allocate( sizeof( ctr_argument ) );
    a->object = CtrStdNil;
    ctr_object* numObject = ctr_send_message( o, "toNumber", 8, a );
    ctr_heap_free(a);
    if ( numObject->info.type != CTR_OBJECT_TYPE_OTNUMBER ) {
        CtrStdFlow = ctr_build_string_from_cstring( "toNumber must return a number." );
        return ctr_build_number_from_float((ctr_number)0);
    }
    return numObject;
}

/**
 * @internal
 *
 * InternalStringCast
 *
 * Casts an object to a string object.
 */
 ctr_object* ctr_internal_cast2string( ctr_object* o ) {
    if ( o->info.type == CTR_OBJECT_TYPE_OTSTRING ) return o;
    ctr_argument* a = ctr_heap_allocate( sizeof( ctr_argument ) );
    a->object = CtrStdNil;
    ctr_object* stringObject;
    // printf("%d, %s\n", o->info.type, o->lexical_name?o->lexical_name->value.svalue->value:"No-lexical-name");
    switch (o->info.type) {
      case CTR_OBJECT_TYPE_OTARRAY:
        stringObject = ctr_array_to_string(o, NULL);
        break;
      case CTR_OBJECT_TYPE_OTBOOL:
        stringObject = ctr_bool_to_string(o, NULL);
        break;
      case CTR_OBJECT_TYPE_OTNUMBER:
        stringObject = ctr_number_to_string(o, NULL);
        break;
      case CTR_OBJECT_TYPE_OTOBJECT:
        stringObject = ctr_reflect_get_primitive_link(o) == CtrStdMap ? ctr_map_to_string(o, NULL) : ctr_send_message( o, "toString", 8, a );
        break;
      default:
        stringObject = ctr_send_message( o, "toString", 8, a );
        break;
    }
    ctr_heap_free(a);
    if ( stringObject->info.type != CTR_OBJECT_TYPE_OTSTRING ) {
        printf("wanted 3, got %d from %d\n", stringObject->info.type, o->info.type);
        CtrStdFlow = ctr_build_string_from_cstring( "toString must return a string." );
        ctr_print_stack_trace();
        return ctr_build_string_from_cstring( "?" );
    }
    return stringObject;
}

/**
 * @internal
 *
 * InternalBooleanCast
 *
 * Casts an object to a boolean.
 */
 ctr_object* ctr_internal_cast2bool( ctr_object* o ) {
    if (o->info.type == CTR_OBJECT_TYPE_OTBOOL) return o;
    ctr_argument* a = ctr_heap_allocate( sizeof( ctr_argument ) );
    a->object = CtrStdNil;
    ctr_object* boolObject = ctr_send_message( o, "toBoolean", 9, a );
    ctr_heap_free(a);
    if ( boolObject->info.type != CTR_OBJECT_TYPE_OTBOOL ) {
        CtrStdFlow = ctr_build_string_from_cstring( "toBoolean must return a boolean." );
        return ctr_build_bool(0);
    }
    return boolObject;
}

/**
 * @internal
 *
 * ContextOpen
 *
 * Opens a new context to keep track of variables.
 */
void ctr_open_context() {
    ctr_object* context;
    if (ctr_context_id >= 99999) {
        CtrStdFlow = ctr_build_string_from_cstring( "Too many nested calls." );
        CtrStdFlow->info.sticky = 1;
    }
    context = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    context->info.sticky = 1;
    ctr_contexts[++ctr_context_id] = context;
}

/**
 * @internal
 *
 * ContextClose
 *
 * Closes a context.
 */
void ctr_close_context() {
    ctr_contexts[ctr_context_id]->info.sticky = 0;
    if (ctr_context_id == 0) return;
    ctr_context_id--;
}

/**
 * @internal
 *
 * CTRFind
 *
 * Tries to locate a variable in the current context or one
 * of the contexts beneath.
 */
ctr_object* ctr_find_(ctr_object* key, int noerror) {
    int i = ctr_context_id;
    ctr_object* foundObject = NULL;
    if (CtrStdFlow) return CtrStdNil;
    while((i>-1 && foundObject == NULL)) {
        ctr_object* context = ctr_contexts[i];
        foundObject = ctr_internal_object_find_property(context, key, 0);
        i--;
    }
    if (foundObject)
      return foundObject;
    ctr_internal_plugin_find(key);
    foundObject = ctr_internal_object_find_property(CtrStdWorld, key, 0);
    if (foundObject == NULL) {
        if (noerror) return NULL;
        char* key_name;
        char* message;
        char* full_message;
        int message_size;
        message = "Key not found: ";
        message_size = ((strlen(message))+key->value.svalue->vlen);
        full_message = ctr_heap_allocate( message_size * sizeof( char ) );
        key_name = ctr_heap_allocate_cstring( key );
        memcpy(full_message, message, strlen(message));
        memcpy(full_message + strlen(message), key_name, key->value.svalue->vlen);
        CtrStdFlow = ctr_build_string(full_message, message_size);
        CtrStdFlow->info.sticky = 1;
        ctr_heap_free( full_message );
        ctr_heap_free( key_name );
        return CtrStdNil;
    }
    return foundObject;
}
ctr_object* ctr_find(ctr_object* key) {
  int i = ctr_context_id;
  ctr_object* foundObject = NULL;
  if (CtrStdFlow) return CtrStdNil;
  while((i>-1 && foundObject == NULL))
      foundObject = ctr_internal_object_find_property(ctr_contexts[i--], key, 0);

  if (foundObject)
    return foundObject;
  ctr_internal_plugin_find(key);
  foundObject = ctr_internal_object_find_property(CtrStdWorld, key, 0);
  if (foundObject == NULL) {
      char* key_name;
      char* message;
      char* full_message;
      int message_size;
      message = "Key not found: ";
      message_size = ((strlen(message))+key->value.svalue->vlen);
      full_message = ctr_heap_allocate( message_size * sizeof( char ) );
      key_name = ctr_heap_allocate_cstring( key );
      memcpy(full_message, message, strlen(message));
      memcpy(full_message + strlen(message), key_name, key->value.svalue->vlen);
      CtrStdFlow = ctr_build_string(full_message, message_size);
      CtrStdFlow->info.sticky = 1;
      ctr_heap_free( full_message );
      ctr_heap_free( key_name );
      return CtrStdNil;
  }
  return foundObject;
}

/**
 * @internal
 *
 * CTRFindInMy
 *
 * Tries to locate a property of an object.
 */
ctr_object* ctr_find_in_my(ctr_object* key) {
    ctr_object* context = ctr_find(ctr_build_string_from_cstring( ctr_clex_keyword_me ) );
    ctr_object* foundObject = ctr_internal_object_find_property(context, key, 0);
    if (CtrStdFlow) return CtrStdNil;
    if (foundObject == NULL) {
        char* key_name;
        char* message;
        char* full_message;
        int message_size;
        message = "Object property not found: ";
        message_size = ((strlen(message))+key->value.svalue->vlen);
        full_message = ctr_heap_allocate( message_size * sizeof( char ) );
        key_name = ctr_heap_allocate_cstring( key );
        memcpy(full_message, message, strlen(message));
        memcpy(full_message + strlen(message), key_name, key->value.svalue->vlen);
        CtrStdFlow = ctr_build_string(full_message, message_size);
        CtrStdFlow->info.sticky = 1;
        ctr_heap_free( full_message );
        ctr_heap_free( key_name );
        return CtrStdNil;
    }
    return foundObject;
}

/**
 * @internal
 *
 * CTRSetBasic
 *
 * Sets a proeprty in an object (context).
 */
void ctr_set(ctr_object* key, ctr_object* object) {
    int i = ctr_context_id;
    ctr_object* context;
    ctr_object* foundObject = NULL;
    if (ctr_contexts[ctr_context_id] == CtrStdWorld) {
        ctr_internal_object_set_property(ctr_contexts[ctr_context_id], key, object, 0);
        return;
    }
    while((i>-1 && foundObject == NULL)) {
        context = ctr_contexts[i];
        foundObject = ctr_internal_object_find_property(context, key, 0);
        if (foundObject) break;
        i--;
    }
    if (!foundObject) {
        char* key_name;
        char* message;
        char* full_message;
        int message_size;
        message = "Cannot assign to undefined variable: ";
        message_size = ((strlen(message))+key->value.svalue->vlen);
        full_message = ctr_heap_allocate(message_size*sizeof(char));
        key_name = ctr_heap_allocate_cstring( key );
        memcpy(full_message, message, strlen(message));
        memcpy(full_message + strlen(message), key_name, key->value.svalue->vlen);
        CtrStdFlow = ctr_build_string(full_message, message_size);
        CtrStdFlow->info.sticky = 1;
        ctr_heap_free( full_message );
        ctr_heap_free( key_name );
        return;
    }
    if(strncmp(key->value.svalue->value, "me", key->value.svalue->vlen) != 0) object->lexical_name = key;
    ctr_internal_object_set_property(context, key, object, 0);
}

ctr_object* ctr_give_version(ctr_object* myself, ctr_argument* argumentList) {
    return ctr_build_string_from_cstring(CTR_VERSION);
}

ctr_object* ctr_object_destruct(ctr_object* object, ctr_argument* nothing) {
  return CtrStdNil;
}

/**
 * @internal
 *
 * WorldInitialize
 *
 * Populate the World of Citron.
 */
void ctr_initialize_world() {
    register_signal_handlers();

    int i;
    srand((unsigned)time(NULL));
    for(i=0; i<16; i++) {
        CtrHashKey[i] = (rand() % 255);
    }
    ctr_first_object = NULL;
    CtrStdWorld = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    CtrStdWorld->info.sticky = 1;
    ctr_contexts[0] = CtrStdWorld;

    ctr_instrumentor_func = NULL; //register instrumentors to nil
    ctr_past_instrumentor_func = NULL; //register instrumentors to nil

    /* Object */
    CtrStdObject = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_NEW ), &ctr_object_make );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_CTOR_NEW ), &ctr_object_ctor );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_GENACC ), &ctr_object_attr_accessor );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_GENREA ), &ctr_object_attr_reader );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_GENWRI ), &ctr_object_attr_writer );
#ifdef CTR_DEBUG_HIDING
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_NEW_HIDING ), &ctr_object_make_hiding );
#endif //CTR_DEBUG_HIDING
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_SWAP ), &ctr_object_swap );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_EQUALS ), &ctr_object_equals );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_SYMBOL_EQUALS ), &ctr_object_equals );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( "id" ), &ctr_object_id );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_ONDO ), &ctr_object_on_do );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_RESPOND_TO ), &ctr_object_respond );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_RESPOND_TO_AND ), &ctr_object_respond_and );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_RESPOND_TO_AND_AND ), &ctr_object_respond_and_and);
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_TYPE ), &ctr_object_type );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_ISNIL ), &ctr_object_is_nil );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_MYSELF ), &ctr_object_myself );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_DO ), &ctr_object_do );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_DONE ), &ctr_object_done );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( "or:" ), &ctr_object_elvis_op );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_IFFALSE ), &ctr_object_if_false );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_IFTRUE ), &ctr_object_if_true );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_MESSAGEARGS), &ctr_object_message );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_LEARN ), &ctr_object_learn_meaning );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_TOSTRING ), &ctr_object_to_string );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_TONUMBER ), &ctr_object_to_number );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_TOBOOL ), &ctr_object_to_boolean );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_SOCK_I6 ), &ctr_command_remote );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_SOCK_P ), &ctr_command_remote );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_SOCK ), &ctr_command_remote );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( "destruct" ), &ctr_object_destruct );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( CTR_DICT_UNPACK ), &ctr_object_assign );
    ctr_internal_create_func( CtrStdObject, ctr_build_string_from_cstring( "iHash" ), &ctr_object_hash );

    ctr_internal_object_add_property( CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_OBJECT ), CtrStdObject, 0 );
    CtrStdObject->link = NULL;
    CtrStdObject->info.sticky = 1;

    /* Nil */
    CtrStdNil = ctr_internal_create_object(CTR_OBJECT_TYPE_OTNIL);
    ctr_internal_object_add_property( CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_NIL ), CtrStdNil, 0 );
    ctr_internal_create_func( CtrStdNil, ctr_build_string_from_cstring( CTR_DICT_ISNIL ), &ctr_nil_is_nil );
    ctr_internal_create_func( CtrStdNil, ctr_build_string_from_cstring( CTR_DICT_TOSTRING ), &ctr_nil_to_string );
    ctr_internal_create_func( CtrStdNil, ctr_build_string_from_cstring( CTR_DICT_TONUMBER ), &ctr_nil_to_number );
    ctr_internal_create_func( CtrStdNil, ctr_build_string_from_cstring( CTR_DICT_TOBOOL ), &ctr_nil_to_boolean );
    ctr_internal_create_func( CtrStdNil, ctr_build_string_from_cstring( CTR_DICT_UNPACK ), &ctr_nil_assign );
    CtrStdNil->link = CtrStdObject;
    CtrStdNil->info.sticky = 1;

    /* Boolean */
    CtrStdBool = ctr_internal_create_object(CTR_OBJECT_TYPE_OTBOOL);
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_IFTRUE ), &ctr_bool_if_true );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_IFFALSE ), &ctr_bool_if_false );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_BREAK ), &ctr_bool_break );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_CONTINUE ), &ctr_bool_continue );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_ELSE ), &ctr_bool_if_false );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_NOT ), &ctr_bool_not );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_AND ), &ctr_bool_and );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_NOR ), &ctr_bool_nor );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_OR ), &ctr_bool_or );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_XOR ), &ctr_bool_xor );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_SYMBOL_EQUALS ),&ctr_bool_eq );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_NOTEQUAL ), &ctr_bool_neq );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_TONUMBER ), &ctr_bool_to_number );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_TOSTRING ), &ctr_bool_to_string );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_FLIP ), &ctr_bool_flip );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_UNPACK ), &ctr_bool_assign );
    ctr_internal_create_func( CtrStdBool, ctr_build_string_from_cstring( CTR_DICT_EITHEROR ), &ctr_bool_either_or );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_BOOLEAN ), CtrStdBool, 0 );
    CtrStdBool->link = CtrStdObject;
    CtrStdBool->info.sticky = 1;

    /* Number */
    CtrStdNumber = ctr_internal_create_object(CTR_OBJECT_TYPE_OTNUMBER);
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_TOSTEPDO ), &ctr_number_to_step_do );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_PLUS ), &ctr_number_add );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_ADD ), &ctr_number_inc );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_MINUS ), &ctr_number_minus );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_SUBTRACT ), &ctr_number_dec );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_MULTIPLIER ),&ctr_number_multiply );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_TIMES ),&ctr_number_times );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_MULTIPLY ),&ctr_number_mul );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_DIVISION ), &ctr_number_divide );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_DIVIDE ), &ctr_number_div );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_GREATER ), &ctr_number_higherThan );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_GREATER_OR_EQUAL ), &ctr_number_higherEqThan );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_LESS ), &ctr_number_lowerThan );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_LESS_OR_EQUAL ), &ctr_number_lowerEqThan );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_SYMBOL_EQUALS ), &ctr_number_eq );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_NOTEQUAL ), &ctr_number_neq );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_MODULO ), &ctr_number_modulo );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_FACTORIAL ), &ctr_number_factorial );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_FLOOR ), &ctr_number_floor );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_CEIL ), &ctr_number_ceil );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_ROUND ), &ctr_number_round );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_ABS ), &ctr_number_abs );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_SIN ), &ctr_number_sin );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_COS ), &ctr_number_cos );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_EXP ), &ctr_number_exp );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_SQRT ), &ctr_number_sqrt );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_TAN ), &ctr_number_tan );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_ATAN ), &ctr_number_atan );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_LOG ), &ctr_number_log );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_POWER ), &ctr_number_pow );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_MIN_ARG ), &ctr_number_min );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_MAX_ARG ), &ctr_number_max );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_ODD ), &ctr_number_odd );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_EVEN ), &ctr_number_even );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_POS ), &ctr_number_positive );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_NEG ), &ctr_number_negative );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_TOSTRING ), &ctr_number_to_string );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_TOBOOL ), &ctr_number_to_boolean );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_TONUMBER ), &ctr_object_myself );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_BETWEEN ),&ctr_number_between );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_SLL ), &ctr_number_shl);
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_SRL ), &ctr_number_shr);
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_INTOR ), &ctr_number_or);
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_INTAND ), &ctr_number_and);
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_INTXOR ), &ctr_number_xor);
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_TOBITS ), &ctr_number_uint_binrep);
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_UNPACK ), &ctr_number_assign);
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_TO_BYTE ),&ctr_number_to_byte );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_QUALIFY ),&ctr_number_qualify );
    ctr_internal_create_func(CtrStdNumber, ctr_build_string_from_cstring( CTR_DICT_RESPOND_TO ),&ctr_number_qualify );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_NUMBER ), CtrStdNumber, 0);
    CtrStdNumber->link = CtrStdObject;
    CtrStdNumber->info.sticky = 1;

    /* String */
    CtrStdString = ctr_internal_create_object(CTR_OBJECT_TYPE_OTSTRING);
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_BYTES ), &ctr_string_bytes );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_LENGTH ), &ctr_string_length );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_FROM_TO ), &ctr_string_fromto );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_FROM_LENGTH ), &ctr_string_from_length );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_PLUS ), &ctr_string_concat );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_SYMBOL_EQUALS ), &ctr_string_eq );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_UNEQUALS ), &ctr_string_neq );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_TRIM ), &ctr_string_trim );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_LEFT_TRIM ), &ctr_string_ltrim );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_RIGHT_TRIM ), &ctr_string_rtrim );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_HTML_ESCAPE ), &ctr_string_html_escape );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_COUNTOF ), &ctr_string_count_of );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_SLICEFROMTO ), &ctr_string_slice );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_AT ), &ctr_string_at );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_AT_SYMBOL ), &ctr_string_at );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_BYTE_AT ), &ctr_string_byte_at );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_INDEX_OF ), &ctr_string_index_of );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_LAST_INDEX_OF ), &ctr_string_last_index_of );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_REPLACE_WITH ), &ctr_string_replace_with );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_SPLIT ), &ctr_string_split );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_ASCII_UPPER_CASE ), &ctr_string_to_upper );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_ASCII_LOWER_CASE ), &ctr_string_to_lower );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_ASCII_UPPER_CASE_1 ), &ctr_string_to_upper1st );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_ASCII_LOWER_CASE_1 ), &ctr_string_to_lower1st );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_SKIP ), &ctr_string_skip );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_APPEND ), &ctr_string_append );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( "multiply:" ), &ctr_string_multiply );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_MULTIPLIER ), &ctr_string_multiply );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_ADD ), &ctr_string_append );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_TO_NUMBER ), &ctr_string_to_number );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_TOBOOL ), &ctr_string_to_boolean );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_FIND_PATTERN_DO ), &ctr_string_find_pattern_do );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_FIND_PATTERN_DO_OPTIONS ), &ctr_string_find_pattern_options_do );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_REGEX_ISPCRE ), &ctr_string_is_regex_pcre );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_CONTAINS_PATTERN ), &ctr_string_contains_pattern );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_HASH_WITH_KEY ), &ctr_string_hash_with_key );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_EVAL ), &ctr_string_eval );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_TOSTRING), &ctr_string_to_string );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_STRFMT ), &ctr_string_format );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_STRFMTMAP ), &ctr_string_format_map );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_ESCAPE_QUOTES ),&ctr_string_quotes_escape );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_CHARACTERS ),&ctr_string_characters );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_TO_BYTE_ARRAY ),&ctr_string_to_byte_array );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_APPEND_BYTE ),&ctr_string_append_byte );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( "charSub:" ), &ctr_string_csub );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( "charAdd:" ), &ctr_string_cadd );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_CONTAINS ),&ctr_string_contains );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( "paddingLeft:" ),&ctr_string_padding_left );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( "paddingRight:" ),&ctr_string_padding_right );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( "randomizeBytesWithLength:" ),&ctr_string_randomize_bytes );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( "reverse" ),&ctr_string_reverse );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( "isConstructible:" ),&ctr_string_is_ctor );
    ctr_internal_create_func(CtrStdString, ctr_build_string_from_cstring( CTR_DICT_UNPACK ),&ctr_string_assign );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_STRING ), CtrStdString, 0 );
    CtrStdString->link = CtrStdObject;
    CtrStdString->info.sticky = 1;

    /* Block */
    CtrStdBlock = ctr_internal_create_object(CTR_OBJECT_TYPE_OTBLOCK);
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_RUN ), &ctr_block_runIt );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_APPLY_TO ), &ctr_block_runIt );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_APPLY_TO_AND ), &ctr_block_runIt );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_APPLY_TO_AND_AND ), &ctr_block_runIt );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_APPLY_TO_AND_AND ), &ctr_block_runIt );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_APPLY_TO_AND_AND "and:" ), &ctr_block_runIt );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_APPLY_TO_AND_AND "and:and:" ), &ctr_block_runIt );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_SET_VALUE ), &ctr_block_set );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_ERROR ), &ctr_block_error );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_CATCH ), &ctr_block_catch );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_WHILE_TRUE ), &ctr_block_while_true );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_WHILE_FALSE ), &ctr_block_while_false );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_TOSTRING ), &ctr_block_to_string );
    ctr_internal_create_func(CtrStdBlock, ctr_build_string_from_cstring( CTR_DICT_UNPACK ), &ctr_block_assign );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_CODE_BLOCK ), CtrStdBlock, 0 );
    CtrStdBlock->link = CtrStdObject;
    CtrStdBlock->info.sticky = 1;

    /* Array */
    CtrStdArray = ctr_array_new(CtrStdObject, NULL);
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_NEW ), &ctr_array_new );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_NEW_ARRAY_AND_PUSH ), &ctr_array_new_and_push );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_NEW_ARRAY_AND_PUSH_SYMBOL ), &ctr_array_new_and_push );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_TYPE ), &ctr_array_type );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_PUSH ), &ctr_array_push );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_PUSH_SYMBOL ), &ctr_array_push );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "fmap:" ), &ctr_array_fmap );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "imap:" ), &ctr_array_imap );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "foldl:accumulator:" ), &ctr_array_foldl );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "filter:" ), &ctr_array_filter );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "reverse" ), &ctr_array_reverse );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_UNSHIFT ), &ctr_array_unshift );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_SHIFT ), &ctr_array_shift );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_COUNT ), &ctr_array_count );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_JOIN ), &ctr_array_join );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_POP ), &ctr_array_pop );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_AT ), &ctr_array_get );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_AT_SYMBOL ), &ctr_array_get );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "indexOf:" ), &ctr_array_index );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "contains:" ), &ctr_array_contains );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_SORT ), &ctr_array_sort );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_PUT_AT ), &ctr_array_put );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_FROM_LENGTH ), &ctr_array_from_length );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "skip:" ), &ctr_array_skip );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "zip" ), &ctr_array_zip );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "zipWith:" ), &ctr_array_zip_with );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_PLUS ), &ctr_array_add );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "head" ), &ctr_array_head );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "tail" ), &ctr_array_tail );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "init" ), &ctr_array_init );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "last" ), &ctr_array_last );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_MAP ), &ctr_array_map );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "map_v:" ), &ctr_array_map_v );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_EACH ), &ctr_array_map );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "each_v:" ), &ctr_array_map_v );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_MIN ), &ctr_array_min );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_MAX ), &ctr_array_max );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_SUM ), &ctr_array_sum );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_PRODUCT ), &ctr_array_product );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "*" ), &ctr_array_multiply );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_TOSTRING ), &ctr_array_to_string );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "fill:with:" ), &ctr_array_fill );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "column:" ), &ctr_array_column );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_UNPACK ), &ctr_array_assign );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( CTR_DICT_SERIALIZE ), &ctr_array_to_string );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "intersperse:" ), &ctr_array_intersperse );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "chunks:" ), &ctr_array_chunks );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "chunks:fill:" ), &ctr_array_chunks );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "every:do:" ), &ctr_array_every_do_fill );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "every:do:fill:" ), &ctr_array_every_do_fill );
    ctr_internal_create_func(CtrStdArray, ctr_build_string_from_cstring( "from:lengthMax:" ), &ctr_array_slice );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_ARRAY ), CtrStdArray, 0 );
    CtrStdArray->link = CtrStdObject;
    CtrStdArray->info.sticky = 1;

    /* Iterator */
    ctr_iter_range = ctr_string_eval(ctr_build_string_from_cstring(
      "{:seed var step is my step. var end_value is my end_value. me endIf: {^seed = end_value.}. ^(seed + step).}"
    ), NULL);
    ctr_iter_range->info.sticky = 1;
    ctr_iter_urange = ctr_string_eval(ctr_build_string_from_cstring(
      "{:seed ^(seed + my step).}"
    ), NULL);
    ctr_iter_range->info.sticky = 1;
    CtrStdIter = ctr_array_new(CtrStdObject, NULL);
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("new"), &ctr_iterator_make );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("setSeed:"), &ctr_iterator_set_seed );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("setFunc:"), &ctr_iterator_set_func );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("rangeFrom:to:step:"), &ctr_iterator_make_range );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("rangeFrom:step:"), &ctr_iterator_make_uncapped_range );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("next"), &ctr_iterator_next );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("each:"), &ctr_iterator_each );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("each_v:"), &ctr_iterator_each_v );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("fmap:"), &ctr_iterator_fmap );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("filter:"), &ctr_iterator_filter );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("count"), &ctr_iterator_count );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("take:"), &ctr_iterator_take );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("endBlock"), &ctr_iterator_takewhile );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("endIf:"), &ctr_iterator_end_check );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("takeWhile:"), &ctr_iterator_takewhile );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("toArray"), &ctr_iterator_to_array );
    ctr_internal_create_func(CtrStdIter, ctr_build_string_from_cstring("skip:"), &ctr_iterator_skip );

    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( "Iterator" ), CtrStdIter, 0 );
    CtrStdIter->link = CtrStdObject;
    CtrStdIter->info.sticky = 1;

    /* Map */
    CtrStdMap = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_NEW ), &ctr_map_new );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_TYPE ), &ctr_map_type );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_PUT_AT ), &ctr_map_put );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( "deleteAt:" ), &ctr_map_rm );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_AT ), &ctr_map_get );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_AT_SYMBOL ), &ctr_map_get );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_COUNT ), &ctr_map_count );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_EACH ), &ctr_map_each );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_MAP ), &ctr_map_each );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( "fmap:" ), &ctr_map_fmap );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( "kvmap:" ), &ctr_map_kvmap );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( "kvlist:" ), &ctr_map_kvlist );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_FLIP ), &ctr_map_flip );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_UNPACK ), &ctr_map_assign );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_TOSTRING ), &ctr_map_to_string );
    ctr_internal_create_func(CtrStdMap, ctr_build_string_from_cstring( CTR_DICT_SERIALIZE ), &ctr_map_to_string );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_MAP_OBJECT ), CtrStdMap, 0 );
    CtrStdMap->link = CtrStdObject;
    CtrStdMap->info.sticky = 1;

    /* Console */
    CtrStdConsole = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_WRITE ), &ctr_console_write );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( "writeln:" ), &ctr_console_writeln );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_WRITE_TEMPLATE_SYMBOL ), &ctr_console_write );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_BRK ), &ctr_console_brk );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_RED ), &ctr_console_red );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_GREEN ), &ctr_console_green );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_YELLOW ), &ctr_console_yellow );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_BLUE ), &ctr_console_blue );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_MAGENTA ), &ctr_console_magenta );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_PURPLE ), &ctr_console_magenta );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_CYAN ), &ctr_console_cyan );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_RESET_COLOR ), &ctr_console_reset );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_TAB ), &ctr_console_tab );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( CTR_DICT_LINE ), &ctr_console_line );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( "clear" ), &ctr_console_clear );
    ctr_internal_create_func(CtrStdConsole, ctr_build_string_from_cstring( "clearLine" ), &ctr_console_clear_line );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_PEN ), CtrStdConsole, 0 );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_PEN_TEMPLATE_SYMBOL ), CtrStdConsole, 0 );
    CtrStdConsole->link = CtrStdObject;
    CtrStdConsole->info.sticky = 1;

    /* File */
    CtrStdFile = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    CtrStdFile->value.rvalue = NULL;
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_NEW_ARG ), &ctr_file_new );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_PATH ), &ctr_file_path );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( "realPath" ), &ctr_file_rpath );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_EXT_PATH ), &ctr_file_stdext_path );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_READ ), &ctr_file_read );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_WRITE ), &ctr_file_write );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_APPEND ), &ctr_file_append );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_EXISTS ), &ctr_file_exists );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_SIZE ), &ctr_file_size );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_DELETE ), &ctr_file_delete );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_INCLUDE ), &ctr_file_include );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_INCLUDEHERE ), &ctr_file_include_here );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_OPEN ), &ctr_file_open );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_CLOSE ), &ctr_file_close );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_READ_BYTES ), &ctr_file_read_bytes );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_WRITE_BYTES ), &ctr_file_write_bytes );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_SEEK ), &ctr_file_seek );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_REWIND ), &ctr_file_seek_rewind );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_END ), &ctr_file_seek_end );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_LOCK ), &ctr_file_lock );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_UNLOCK ), &ctr_file_unlock );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_LIST ), &ctr_file_list );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( "typeOf:" ), &ctr_file_type );
    ctr_internal_create_func(CtrStdFile, ctr_build_string_from_cstring( CTR_DICT_TMPFILE ), &ctr_file_tmp );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_FILE ), CtrStdFile, 0);
    CtrStdFile->link = CtrStdObject;
    CtrStdFile->info.sticky = 1;

    /* Command */
    CtrStdCommand = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_ARGUMENT ), &ctr_command_argument );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_ARGUMENT_COUNT ), &ctr_command_num_of_args );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_ENVIRONMENT_VARIABLE ), &ctr_command_get_env );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_SET_ENVIRONMENT_VARIABLE ), &ctr_command_set_env );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_WAIT_FOR_INPUT ), &ctr_command_waitforinput );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_GETC ), &ctr_command_getc );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_INPUT ), &ctr_command_input );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( "setInterruptHandler:" ), &ctr_command_set_INT_handler );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_EXIT ), &ctr_command_exit );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_FLUSH ), &ctr_command_flush );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_FORBID_SHELL ), &ctr_command_forbid_shell );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_FORBID_FILE_WRITE ), &ctr_command_forbid_file_write );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_FORBID_FILE_READ ), &ctr_command_forbid_file_read );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_FORBID_INCLUDE ), &ctr_command_forbid_include );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_FORBID_FORK ), &ctr_command_forbid_fork );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_SET_REMAINING_MESSAGES ), &ctr_command_countdown );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_NEW_SET ), &ctr_command_fork );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_MESSAGE ), &ctr_command_message );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_LISTEN ), &ctr_command_listen );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_JOIN_PROCESS ), &ctr_command_join );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_LOG_SET ), &ctr_command_log );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_WARNING ), &ctr_command_warn );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_ALERT ), &ctr_command_crit );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_ERROR ), &ctr_command_err );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_PID ), &ctr_command_pid );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( "signal:" ), &ctr_command_sig );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_SERVE ), &ctr_command_accept );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_CONN_LIMIT ), &ctr_command_accept_number );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( CTR_DICT_PORT ), &ctr_command_default_port );
    ctr_internal_create_func(CtrStdCommand, ctr_build_string_from_cstring( "changeDirectory:" ), &ctr_command_chdir );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_PROGRAM ), CtrStdCommand, 0 );
    CtrStdCommand->link = CtrStdObject;
    CtrStdCommand->info.sticky = 1;

    /* Clock */
    CtrStdClock = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_WAIT ), &ctr_clock_wait );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_TIME ), &ctr_clock_time );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_NEW_SET ), &ctr_clock_new_set );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_NEW ), &ctr_clock_new );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_DAY ), &ctr_clock_day );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_SET_DAY ), &ctr_clock_set_day );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_MONTH ), &ctr_clock_month );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_SET_MONTH ), &ctr_clock_set_month );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_YEAR ), &ctr_clock_year );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_SET_YEAR ), &ctr_clock_set_year );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_HOUR ), &ctr_clock_hour );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_SET_HOUR ), &ctr_clock_set_hour );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_MINUTE ), &ctr_clock_minute );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_SET_MINUTE ), &ctr_clock_set_minute );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_SECOND ), &ctr_clock_second );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_SET_SECOND ), &ctr_clock_set_second );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_WEEK_DAY ), &ctr_clock_weekday );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_YEAR_DAY ), &ctr_clock_yearday );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_WEEK ), &ctr_clock_week );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_ZONE ), &ctr_clock_get_zone );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_ZONE_SET ), &ctr_clock_set_zone );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_FORMAT ), &ctr_clock_format );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_LIKE ), &ctr_clock_like );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_TOSTRING ), &ctr_clock_to_string );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_ADD_SET ), &ctr_clock_add );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( CTR_DICT_SUBTRACT_SET ), &ctr_clock_subtract );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( "processorClock" ), &ctr_clock_processor_time );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( "ticksPerSecond" ), &ctr_clock_processor_ticks_ps );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( "timeExecutionOf:" ), &ctr_clock_time_exec );
    ctr_internal_create_func(CtrStdClock, ctr_build_string_from_cstring( "executionSeconds:" ), &ctr_clock_time_exec_s );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_CLOCK ), CtrStdClock, 0 );
    ctr_clock_init( CtrStdClock );
    CtrStdClock->link = CtrStdObject;
    CtrStdClock->info.sticky = 1;

    /* Dice */
    CtrStdDice = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    ctr_internal_create_func(CtrStdDice, ctr_build_string_from_cstring( CTR_DICT_ROLL ), &ctr_dice_throw );
    ctr_internal_create_func(CtrStdDice, ctr_build_string_from_cstring( CTR_DICT_ROLL_WITH_SIDES ), &ctr_dice_sides );
    ctr_internal_create_func(CtrStdDice, ctr_build_string_from_cstring( CTR_DICT_RAW_RANDOM_NUMBER ), &ctr_dice_rand );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_DICE ), CtrStdDice, 0 );
    CtrStdDice->link = CtrStdObject;
    CtrStdDice->info.sticky = 1;

    /* Slurp */
    CtrStdSlurp = ctr_internal_create_object( CTR_OBJECT_TYPE_OTOBJECT );
    ctr_internal_create_func(CtrStdSlurp, ctr_build_string_from_cstring( CTR_DICT_OBTAIN ), &ctr_slurp_obtain );
    ctr_internal_create_func(CtrStdSlurp, ctr_build_string_from_cstring( CTR_DICT_RESPOND_TO ), &ctr_slurp_respond_to );
    ctr_internal_create_func( CtrStdSlurp, ctr_build_string_from_cstring( CTR_DICT_RESPOND_TO_AND ), &ctr_slurp_respond_to_and );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_SLURP ), CtrStdSlurp, 0 );
    CtrStdSlurp->link = CtrStdObject;
    CtrStdSlurp->info.sticky = 1;

    /* Shell */
    CtrStdShell = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    ctr_internal_create_func(CtrStdShell, ctr_build_string_from_cstring( CTR_DICT_RESPOND_TO ), &ctr_shell_respond_to );
    ctr_internal_create_func(CtrStdShell, ctr_build_string_from_cstring( CTR_DICT_RESPOND_TO_AND ), &ctr_shell_respond_to_and );
    ctr_internal_create_func(CtrStdShell, ctr_build_string_from_cstring( CTR_DICT_CALL ), &ctr_shell_call );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_SHELL ), CtrStdShell, 0 );
    CtrStdShell->link = CtrStdObject;
    CtrStdShell->info.sticky = 1;

    /* Broom */
    CtrStdGC = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( CTR_DICT_SWEEP ), &ctr_gc_collect );
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( CTR_DICT_SWEEP ":"), &ctr_gc_sweep_this );
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( CTR_DICT_DUST ), &ctr_gc_dust );
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( CTR_DICT_OBJECT_COUNT ), &ctr_gc_object_count );
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( CTR_DICT_KEPT_COUNT ), &ctr_gc_kept_count );
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( CTR_DICT_KEPT_ALLOC ), &ctr_gc_kept_alloc );
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( CTR_DICT_STICKY_COUNT ), &ctr_gc_sticky_count );
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( CTR_DICT_MEMORY_LIMIT ), &ctr_gc_setmemlimit );
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( CTR_DICT_MODE ),  &ctr_gc_setmode );
    ctr_internal_create_func(CtrStdGC, ctr_build_string_from_cstring( "autoAlloc:" ),  &ctr_gc_setautoalloc );
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring( CTR_DICT_BROOM ), CtrStdGC, 0 );
    CtrStdGC->link = CtrStdObject;
    CtrStdGC->info.sticky = 1;

    CtrStdReflect = ctr_reflect_new(CtrStdObject, NULL);

    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("addGlobalVariable:"), &ctr_reflect_add_glob);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("addLocalVariable:"), &ctr_reflect_add_local);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("addPrivateVariable:"), &ctr_reflect_add_my);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("set:to:"), &ctr_reflect_set_to);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("getContext"), &ctr_reflect_dump_context);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("getMethodsOf:"), &ctr_reflect_dump_context_spec);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("getPropertiesOf:"), &ctr_reflect_dump_context_spec_prop);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("getObject:"), &ctr_reflect_find_obj);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("objectExists:"), &ctr_reflect_find_obj_ex);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("argumentListOf:"), &ctr_reflect_cb_ac);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("addArgumentTo:named:"), &ctr_reflect_cb_add_param);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("copyBlock:"), &ctr_reflect_fn_copy);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("newSharedObject"), &ctr_reflect_share_memory);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("link:to:"), &ctr_reflect_link_to);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("parentOf:"), &ctr_reflect_get_first_link);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("isObject:linkedTo:"), &ctr_reflect_is_linked_to);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("isObject:childOf:"), &ctr_reflect_child_of);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("generateLinkTree:"), &ctr_reflect_generate_inheritance_tree);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("typeOf:"), &ctr_reflect_describe_type);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("strTypeOf:"), &ctr_reflect_type_descriptor_print);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("unpack:to:"), &ctr_reflect_bind);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("getResponder:ofObject:"), &ctr_reflect_get_responder);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("closure:of:"), &ctr_reflect_closure_of);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("run:forObject:arguments:"), &ctr_reflect_run_for_object);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("runHere:forObject:arguments:"), &ctr_reflect_run_for_object_ctx);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("primitiveLinkOf:"), &ctr_reflect_get_primitive);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("getProperty:ofObject:"), &ctr_reflect_get_property);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("object:hasOwnResponder:"), &ctr_reflect_has_own_responder);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("disableInstrumentation"), &ctr_reflect_rawmsg);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("enableInstrumentation"), &ctr_reflect_instrmsg);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("registerInstrumentor:"), &ctr_reflect_register_instrumentor);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("currentInstrumentor"), &ctr_reflect_get_instrumentor);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("runAtGlobal:arguments:"), &ctr_reflect_run_glob);
    ctr_internal_create_func(CtrStdReflect, ctr_build_string_from_cstring("version"), &ctr_give_version);

    ctr_internal_create_func(CtrStdObject, ctr_build_string_from_cstring("&method:"), &ctr_reflect_object_delegate_get_responder);
    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring("Reflect"), CtrStdReflect, 0);
    CtrStdReflect->info.sticky = 1;

    CtrStdReflect_cons = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    CtrStdReflect_cons->link = CtrStdObject;

    ctr_internal_create_func(CtrStdReflect_cons, ctr_build_string_from_cstring("of:"), &ctr_reflect_cons_of);
    ctr_internal_create_func(CtrStdReflect_cons, ctr_build_string_from_cstring("value"), &ctr_reflect_cons_value);
    ctr_internal_create_func(CtrStdReflect_cons, ctr_build_string_from_cstring("toString"), &ctr_reflect_cons_str);

    ctr_internal_object_add_property(CtrStdWorld, ctr_build_string_from_cstring("%ctor"), CtrStdReflect_cons, 0);
    CtrStdReflect_cons->info.sticky = 1;

    // importlib
    CtrStdImportLib = ctr_importlib_begin(CtrStdObject, NULL);

    // Fiber
    ctr_fiber_begin_init();

    /* Other objects */
    CtrStdBreak = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    CtrStdContinue = ctr_internal_create_object(CTR_OBJECT_TYPE_OTOBJECT);
    CtrStdExit = ctr_internal_create_object( CTR_OBJECT_TYPE_OTOBJECT );
    CtrStdBreak->info.sticky = 1;
    CtrStdContinue->info.sticky = 1;
    CtrStdExit->info.sticky = 1;

    /* Construct the whitelist for eval */
    ctr_secpro_eval_whitelist[0] = ctr_array_push;
    ctr_secpro_eval_whitelist[1] = ctr_map_new;
    ctr_secpro_eval_whitelist[2] = ctr_map_put;
    ctr_secpro_eval_whitelist[3] = ctr_array_new;
    ctr_secpro_eval_whitelist[4] = ctr_nil_to_string;
    ctr_secpro_eval_whitelist[5] = ctr_bool_to_string;
    ctr_secpro_eval_whitelist[6] = ctr_number_to_string;
    ctr_secpro_eval_whitelist[7] = ctr_string_to_string;
    ctr_secpro_eval_whitelist[8] = ctr_array_new_and_push;

    /* relax eval a bit */
    ctr_secpro_eval_whitelist[9] = ctr_number_add;
    ctr_secpro_eval_whitelist[10] = ctr_number_minus;
    ctr_secpro_eval_whitelist[11] = ctr_number_divide;
    ctr_secpro_eval_whitelist[12] = ctr_number_multiply;
    ctr_secpro_eval_whitelist[13] = ctr_number_sqrt;
    ctr_secpro_eval_whitelist[14] = ctr_number_pow;

    /* maximum number of connections to accept (in total) */
    ctr_accept_n_connections = 0;
}

ctr_object* ctr_get_responder(ctr_object* receiverObject, char* message, long vlen) {
  ctr_object* methodObject;
  ctr_object* searchObject;
  methodObject = NULL;
  searchObject = receiverObject;
  int toParent = 0;
  ctr_object* me;
  ctr_object* msg = NULL;
  if (vlen > 1 && message[0] == '`') {
      me = ctr_internal_object_find_property(ctr_contexts[ctr_context_id], ctr_build_string_from_cstring( ctr_clex_keyword_me ), 0);
      if (searchObject == me) {
          toParent = 1;
          message = message + 1;
          vlen--;
      }
  }
  msg = ctr_build_string(message, vlen);
  msg->info.sticky = 1; /* prevent message from being swept, no need to free(), GC will do */
  while(!methodObject) {
      methodObject = ctr_internal_object_find_property(searchObject, msg, 1);
      if (methodObject && toParent) { toParent = 0; methodObject = NULL; }
      if (methodObject) break;
      if (!searchObject->link) break;
      searchObject = searchObject->link;
  }
  return methodObject;
}

/**
 * @internal
 *
 * CTRTransformArgumentList2CTRList
 *
 * Converts an argumentList to a citron list
 */
ctr_object* ctr_internal_argumentptr2tuple(ctr_argument* argumentList) {
  ctr_object* ret = ctr_array_new(CtrStdArray, NULL);
  while(argumentList->object) {
    ctr_array_push(ret, argumentList);
    argumentList = argumentList->next;
    if(!argumentList) break;
  }
  return ret;
}
/**
 * @internal
 *
 * CTRMessageSend
 *
 * Sends a message to a receiver object.
 */
__attribute__((optimize(0))) ctr_object* ctr_send_message(ctr_object* receiverObject, char* message, long vlen, ctr_argument* argumentList) {
    if(unlikely(receiverObject != CtrStdReflect && ctr_instrumentor_func != NULL)) {
      ctr_argument* blkargs = ctr_heap_allocate(sizeof(ctr_argument));
      blkargs->object = receiverObject;
      blkargs->next = ctr_heap_allocate(sizeof(ctr_argument));
      blkargs->next->object = ctr_build_string(message, vlen);
      blkargs->next->next = ctr_heap_allocate(sizeof(ctr_argument));
      blkargs->next->next->object = ctr_internal_argumentptr2tuple(argumentList);
      ctr_object* result = ctr_block_run(ctr_instrumentor_func, blkargs, ctr_instrumentor_func);
      if(result == ctr_instrumentor_func) goto no_instrum;
      return result;
    }
    no_instrum:;
    char toParent = 0;
    int  i = 0;
    char messageApproved = 0;
    ctr_object* me;
    ctr_object* methodObject;
    ctr_object* searchObject;
    ctr_object* returnValue;
    ctr_argument* argCounter;
    ctr_argument* mesgArgument;
    ctr_object* result;
    ctr_object* (*funct)(ctr_object* receiverObject, ctr_argument* argumentList);
    ctr_object* msg = ctr_build_string(message, vlen);
    int argCount;
    if (CtrStdFlow != NULL) return CtrStdNil; /* Error mode, ignore subsequent messages until resolved. */
    if ( ctr_command_security_profile & CTR_SECPRO_COUNTDOWN ) {
        if ( ctr_command_tick > ctr_command_maxtick ) {
            printf( "This program has exceeded the maximum number of messages.\n" );
            exit(1);
        }
        ctr_command_tick += 1;
    }
    methodObject = ctr_get_responder(receiverObject, message, vlen);
    if (!methodObject) {
      if(strcmp(message, "respondTo:") == 0) {
        // printf("Requested message to catch-all in:\n");
        // ctr_print_stack_trace();
        return receiverObject;
      }
        argCounter = argumentList;
        argCount = 0;
        while(argCounter && argCounter->next && argCount < 4) {
            argCounter = argCounter->next;
            argCount ++;
        }
        mesgArgument = (ctr_argument*) ctr_heap_allocate( sizeof( ctr_argument ) );
        mesgArgument->object = ctr_build_string(message, vlen);
        mesgArgument->next = argumentList;
        if (argCount == 0 || argCount > 2) {
            returnValue = ctr_send_message(receiverObject, CTR_DICT_RESPOND_TO, strlen(CTR_DICT_RESPOND_TO),  mesgArgument);
        } else if (argCount == 1) {
            returnValue = ctr_send_message(receiverObject, CTR_DICT_RESPOND_TO_AND, strlen(CTR_DICT_RESPOND_TO_AND),  mesgArgument);
        } else if (argCount == 2) {
            returnValue = ctr_send_message(receiverObject, CTR_DICT_RESPOND_TO_AND_AND, strlen(CTR_DICT_RESPOND_TO_AND_AND),  mesgArgument);
        }
        ctr_heap_free( mesgArgument );
        msg->info.sticky = 0;
        if (receiverObject->info.chainMode == 1) return receiverObject;
        return returnValue;
    }
    if (methodObject->info.type == CTR_OBJECT_TYPE_OTNATFUNC) {
        funct = methodObject->value.fvalue;
#ifdef EVALSECURITY
        if ( ctr_command_security_profile & CTR_SECPRO_EVAL ) {
            messageApproved = 0;
            for ( i = 0; i < 15; i ++ ) {
                if ( funct == ctr_secpro_eval_whitelist[i] ) {
                    messageApproved = 1;
                    break;
                }
            }
            if ( !messageApproved ) {
                printf( "Native message not allowed in eval %s.\n", msg->value.svalue->value );
                ctr_print_stack_trace();
                exit(1);
            }
        }
#endif

        result = funct(receiverObject, argumentList);
    }
    if (methodObject->info.type == CTR_OBJECT_TYPE_OTBLOCK ) {
      #ifdef EVALSECURITY
        if ( ctr_command_security_profile & CTR_SECPRO_EVAL ) {
            printf( "Custom message not allowed in eval.\n" );
            ctr_print_stack_trace();
            exit(1);
        }
        #endif
        result = ctr_block_run(methodObject, argumentList, receiverObject);
    }
    if (msg) msg->info.sticky = 0;
    if (receiverObject->info.chainMode == 1) return receiverObject;
    return result; //Normally cascade down to native functions, so get the return type
}


/**
 * @internal
 *
 * CTRValueAssignment
 *
 * Assigns a value to a variable in the current context.
 */
ctr_object* ctr_assign_value(ctr_object* key, ctr_object* o) {
    ctr_object* object = NULL;
    if (CtrStdFlow) return CtrStdNil;
    key->info.sticky = 0;
    switch(o->info.type){
        case CTR_OBJECT_TYPE_OTBOOL:
            object = ctr_build_bool(o->value.bvalue);
            break;
        case CTR_OBJECT_TYPE_OTNUMBER:
            object = ctr_build_number_from_float(o->value.nvalue);
            break;
        case CTR_OBJECT_TYPE_OTSTRING:
            object = ctr_build_string(o->value.svalue->value, o->value.svalue->vlen);
            break;
        case CTR_OBJECT_TYPE_OTNIL:
        case CTR_OBJECT_TYPE_OTNATFUNC:
        case CTR_OBJECT_TYPE_OTOBJECT:
        case CTR_OBJECT_TYPE_OTEX:
        case CTR_OBJECT_TYPE_OTMISC:
        case CTR_OBJECT_TYPE_OTARRAY:
        case CTR_OBJECT_TYPE_OTBLOCK:
            object = o;
            break;
    }
    ctr_set(key, object);
    return object;
}

ctr_object* ctr_const_assign_value(ctr_object* key, ctr_object* o, ctr_object* context) {
    if (CtrStdFlow) return CtrStdNil;
    ctr_open_context();
    ctr_object* object = o;
    key->info.sticky = 0;
    ctr_set(key, object);
    ctr_internal_object_add_property(ctr_contexts[ctr_context_id], ctr_build_string_from_cstring("context"), context, 0);
    context = ctr_contexts[ctr_context_id];
    ctr_close_context();
    return context;
}

/**
 * @internal
 *
 * CTRAssignValueObject
 *
 * Assigns a value to a property of an object.
 */
ctr_object* ctr_assign_value_to_my(ctr_object* key, ctr_object* o) {
    ctr_object* object = NULL;
    ctr_object* my = ctr_find(ctr_build_string_from_cstring( ctr_clex_keyword_me ) );
    if (CtrStdFlow) return CtrStdNil;
    key->info.sticky = 0;
    switch(o->info.type){
        case CTR_OBJECT_TYPE_OTBOOL:
            object = ctr_build_bool(o->value.bvalue);
            break;
        case CTR_OBJECT_TYPE_OTNUMBER:
            object = ctr_build_number_from_float(o->value.nvalue);
            break;
        case CTR_OBJECT_TYPE_OTSTRING:
            object = ctr_build_string(o->value.svalue->value, o->value.svalue->vlen);
            break;
        case CTR_OBJECT_TYPE_OTNIL:
        case CTR_OBJECT_TYPE_OTNATFUNC:
        case CTR_OBJECT_TYPE_OTOBJECT:
        case CTR_OBJECT_TYPE_OTEX:
        case CTR_OBJECT_TYPE_OTMISC:
        case CTR_OBJECT_TYPE_OTARRAY:
        case CTR_OBJECT_TYPE_OTBLOCK:
            object = o;
            break;
    }
    ctr_internal_object_set_property(my, key, object, 0);
    return object;
}

/**
 * @internal
 *
 * CTRAssignValueObjectLocal
 *
 * Assigns a value to a local of an object.
 */
ctr_object* ctr_assign_value_to_local(ctr_object* key, ctr_object* o) {
    ctr_object* object = NULL;
    ctr_object* context;
    if (CtrStdFlow) return CtrStdNil;
    context = ctr_contexts[ctr_context_id];
    key->info.sticky = 0;
    switch(o->info.type){
        case CTR_OBJECT_TYPE_OTBOOL:
            object = ctr_build_bool(o->value.bvalue);
            break;
        case CTR_OBJECT_TYPE_OTNUMBER:
            object = ctr_build_number_from_float(o->value.nvalue);
            break;
        case CTR_OBJECT_TYPE_OTSTRING:
            object = ctr_build_string(o->value.svalue->value, o->value.svalue->vlen);
            break;
        case CTR_OBJECT_TYPE_OTNIL:
        case CTR_OBJECT_TYPE_OTNATFUNC:
        case CTR_OBJECT_TYPE_OTOBJECT:
        case CTR_OBJECT_TYPE_OTEX:
        case CTR_OBJECT_TYPE_OTMISC:
        case CTR_OBJECT_TYPE_OTARRAY:
        case CTR_OBJECT_TYPE_OTBLOCK:
            object = o;
            break;
    }
    ctr_internal_object_set_property(context, key, object, 0);
    return object;
}

/**
 * @internal
 *
 * CTRAssignValueObjectLocalByRef
 *
 * Assigns a value to a local of an object.
 * Always assigns by reference.
 */
ctr_object* ctr_assign_value_to_local_by_ref(ctr_object* key, ctr_object* o) {
    ctr_object* object = NULL;
    ctr_object* context;
    if (CtrStdFlow) return CtrStdNil;
    context = ctr_contexts[ctr_context_id];
    key->info.sticky = 0;
    object = o;
    ctr_internal_object_set_property(context, key, object, 0);
    return object;
}
