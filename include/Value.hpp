#pragma once
#include <string>
#include <variant>
#include <vector>
namespace CatVM {
    struct Value;
    // all possible value types
    enum class ValueType {
        NONE,
        BOOL,
        INT,
        DOUBLE,
        OBJECT,
    };
    // all possible object types: string, array, map, function, class, etc.
    enum class ObjectType {
        NONE,
        STRING,
        CODE,
    };

    struct Object {
        Object() : type(ObjectType::NONE) {}
        Object(ObjectType type) : type(type) {}

        ObjectType type;
    };

    struct StringObject : public Object {
        StringObject() : Object(ObjectType::STRING), string() {}
        StringObject(const std::string &string_) : Object(ObjectType::STRING), string(string_) {}
        std::string string;
    };

    struct CodeObject : public Object {
        CodeObject() : Object(ObjectType::CODE), name() {}
        CodeObject(const std::string &name_) : Object(ObjectType::CODE), name(name_) {}

        std::string name;              // name of the unit(usually the function name)
        std::vector<uint8_t> bytecodes;// bytecode
        std::vector<Value> constants;  // constant pool
    };

    struct Value {
        ValueType type;
        union {
            bool boolean;
            int integer;
            double doubleing;
            Object *object;
        } as;
    };


// ----------------------------------
// constructors Value
// ----------------------------------
#define BOOL(value) (CatVM::Value{CatVM::ValueType::BOOL, {.boolean = value}})
#define INT(value) (CatVM::Value{CatVM::ValueType::INT, {.integer = value}})
#define DOUBLE(value) (CatVM::Value{CatVM::ValueType::DOUBLE, {.doubleing = value}})
// ----------------------------------
// constructors Object(allocate memory on the heap)
// ----------------------------------
// use raw pointer to manage memory so that we can apply GC
// #define ALLOC_STRING(value) (CatVM::Value{CatVM::ValueType::OBJECT, {.object = (Object *) new CatVM::StringObject(value)}})
#define ALLOC_STRING(value)                                              \
    ([&]() -> CatVM::Value {                                             \
        auto temp = new CatVM::StringObject(value);                      \
        return CatVM::Value{CatVM::ValueType::OBJECT, {.object = temp}}; \
    })()

#define ALLOC_CODE(name)                                                 \
    ([&]() -> CatVM::Value {                                             \
        auto temp = new CatVM::CodeObject(name);                         \
        return CatVM::Value{CatVM::ValueType::OBJECT, {.object = temp}}; \
    })()

// ----------------------------------
// accessors Value and Object
// ----------------------------------
#define AS_BOOL(cat_value) ((bool) (cat_value.as.boolean))
#define AS_INT(cat_value) ((int) (cat_value.as.integer))
#define AS_DOUBLE(cat_value) ((double) (cat_value.as.doubleing))
#define AS_OBJECT(cat_value) ((CatVM::Object *) (cat_value.as.object))      // cast to object
#define AS_STRING(cat_value) ((CatVM::StringObject *) (cat_value.as.object))// cast to string object
#define AS_CPP_STRING(cat_value) (AS_STRING(cat_value)->string)             // cast to string object and get the string value
#define AS_CODE(cat_value) ((CatVM::CodeObject *) (cat_value.as.object))    // cast to code object
// ----------------------------------
// check type
// ----------------------------------
#define IS_BOOL(cat_value) ((cat_value.type == CatVM::ValueType::BOOL))
#define IS_INT(cat_value) ((cat_value.type == CatVM::ValueType::INT))
#define IS_DOUBLE(cat_value) ((cat_value.type == CatVM::ValueType::DOUBLE))
#define IS_OBJECT(cat_value) ((cat_value.type == CatVM::ValueType::OBJECT))
#define IS_OBJECT_TYPE(cat_value, objectType) \
    (IS_OBJECT(cat_value) && (AS_OBJECT(cat_value)->type == objectType))
#define IS_STRING(cat_value) (IS_OBJECT_TYPE(cat_value, CatVM::ObjectType::STRING))
#define IS_CODE(cat_value) (IS_OBJECT_TYPE(cat_value, CatVM::ObjectType::CODE))


}// namespace CatVM
