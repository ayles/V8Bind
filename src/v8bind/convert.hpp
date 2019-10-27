//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_CONVERT_HPP
#define SANDWICH_V8B_CONVERT_HPP

#include <v8bind/class.hpp>
#include <v8bind/traits.hpp>

#include <v8.h>

#include <type_traits>
#include <string>
#include <string_view>
#include <stdexcept>
#include <exception>
#include <memory>
#include <locale>
#include <codecvt>

namespace v8b {

template<typename T, typename Enable = void>
struct Convert;

template<typename T>
struct Convert<v8::Local<T>> {
    using CType = v8::Local<T>;
    using V8Type = v8::Local<T>;

    static bool IsValid(v8::Isolate *, v8::Local<v8::Value> value) {
        return !value.As<T>().IsEmpty();
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not of type");
        }
        return value.As<T>();
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return value;
    }
};

template<>
struct Convert<bool> {
    using CType = bool;
    using V8Type = v8::Local<v8::Boolean>;

    static bool IsValid(v8::Isolate *, v8::Local<v8::Value> value) {
        return !value.IsEmpty() && value->IsBoolean();
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid bool");
        }
        return value.As<v8::Boolean>()->Value();
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return v8::Boolean::New(isolate, value);
    }
};

template<typename T>
struct Convert<T, std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>> {
    using CType = T;
    using V8Type = v8::Local<v8::Number>;

    static bool IsValid(v8::Isolate *, v8::Local<v8::Value> value) {
        return !value.IsEmpty() && value->IsNumber();
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid number");
        }
        return static_cast<T>(value.As<v8::Number>()->Value());
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return v8::Number::New(isolate, static_cast<double>(value));
    }
};

template<typename T>
struct Convert<T, std::enable_if_t<std::is_enum_v<T>>> {
    using CType = T;
    using V8Type = v8::Local<v8::Number>;

    static bool IsValid(v8::Isolate *, v8::Local<v8::Value> value) {
        return !value.IsEmpty() && value->IsNumber();
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid number");
        }
        return static_cast<T>(std::underlying_type_t<T>(value.As<v8::Number>()->Value()));
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return v8::Number::New(isolate, static_cast<double>(std::underlying_type_t<T>(value)));
    }
};

template<typename Char, typename Traits, typename Alloc>
struct Convert<std::basic_string<Char, Traits, Alloc>> {
    static_assert(sizeof(Char) <= sizeof(uint32_t),
                  "Only UTF-8, UTF-16 and UTF-32 strings are supported");

    using CType = std::basic_string<Char, Traits, Alloc>;
    using V8Type = v8::Local<v8::String>;

    static bool IsValid(v8::Isolate *, v8::Local<v8::Value> value) {
        return !value.IsEmpty() && value->IsString();
    }

    static CType FromV8(v8::Isolate* isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid string");
        }
        if constexpr (sizeof(Char) == 1) {
            const v8::String::Utf8Value str(isolate, value);
            return CType(reinterpret_cast<const Char *>(*str));
        } else if constexpr (sizeof(Char) == 2) {
            const v8::String::Value str(isolate, value);
            return CType(reinterpret_cast<const Char *>(*str));
        } else if constexpr (sizeof(Char) == 4) {
            const v8::String::Utf8Value str(isolate, value);
            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> cvt;
            return cvt.from_bytes(*str);
        }
    }

    static V8Type ToV8(v8::Isolate* isolate, const CType &value) {
        if constexpr (sizeof(Char) == 1) {
            return v8::String::NewFromUtf8(
                isolate,
                reinterpret_cast<const char *>(value.data()),
                v8::NewStringType::kNormal,
                static_cast<int>(value.size())
            ).ToLocalChecked();
        } else if constexpr (sizeof(Char) == 2) {
            return v8::String::NewFromTwoByte(
                isolate,
                reinterpret_cast<uint16_t const*>(value.data()),
                v8::NewStringType::kNormal,
                static_cast<int>(value.size())
            ).ToLocalChecked();
        } else if constexpr (sizeof(Char) == 4) {
            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> cvt;
            auto str = cvt.to_bytes(value.data());
            return v8::String::NewFromUtf8(
                isolate,
                reinterpret_cast<const char *>(str.data()),
                v8::NewStringType::kNormal,
                static_cast<int>(str.size())
            ).ToLocalChecked();
        }
    }
};

template<>
struct Convert<const char *> : Convert<std::string> {};

template<>
struct Convert<const char16_t *> : Convert<std::u16string> {};

template<>
struct Convert<const char32_t *> : Convert<std::u32string> {};

template<typename T>
struct IsWrappedClass : std::is_class<T> {};

template<typename T>
struct IsWrappedClass<v8::Local<T>> : std::false_type {};

template<typename T>
struct IsWrappedClass<v8::Global<T>> : std::false_type {};

template<typename Char, typename Traits>
struct IsWrappedClass<std::basic_string_view<Char, Traits>> : std::false_type {};

template<typename Char, typename Traits, typename Alloc>
struct IsWrappedClass<std::basic_string<Char, Traits, Alloc>> : std::false_type {};

template<typename T>
struct IsWrappedClass<std::shared_ptr<T>> : std::false_type {};


template<typename T>
struct Convert<T *, typename std::enable_if_t<IsWrappedClass<T>::value>> {
    using CType = T *;
    using V8Type = v8::Local<v8::Object>;

    static bool IsValid(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (value.IsEmpty() || !value->IsObject()) {
            return false;
        }
        try {
            Class<std::remove_cv_t<T>>::UnwrapObject(isolate, value);
        } catch (const std::exception &) {
            // JS array converting
            if constexpr (traits::is_vector_v<std::remove_cv_t<T>>) {
                if (value->IsArray()) {
                    auto obj = value.As<v8::Array>();
                    auto context = isolate->GetCurrentContext();
                    T *vec = new T;
                    for (unsigned int i = 0; i < obj->Length(); ++i) {
                        vec->emplace_back(Convert<typename T::value_type>::FromV8(isolate, (obj->Get(context, i).ToLocalChecked())));
                    }
                    obj->SetPrototype(context, Class<std::remove_cv_t<T>>::WrapObject(isolate, vec, true));
                    return true;
                }
            }
            return false;
        }
        return true;
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid object");
        }
        return Class<std::remove_cv_t<T>>::UnwrapObject(isolate, value);
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return Class<std::remove_cv_t<T>>::FindObject(isolate, value);
    }
};

template<typename T>
struct Convert<T, typename std::enable_if_t<IsWrappedClass<T>::value>> {
    using CType = T &;
    using V8Type = v8::Local<v8::Object>;

    static bool IsValid(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        return Convert<T *>::IsValid(isolate, value);
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        auto ptr = Convert<T *>::FromV8(isolate, value);
        if (!ptr) {
            throw std::runtime_error("Failed to unwrap object");
        }
        return *ptr;
    }

    static V8Type ToV8(v8::Isolate *isolate, const T &value) {
        auto wrapped = Class<std::remove_cv_t<T>>::FindObject(isolate, value);
        if (wrapped.IsEmpty()) {
            throw std::runtime_error("Failed to wrap object");
        }
        return wrapped;
    }
};

template<typename T>
struct Convert<std::shared_ptr<T>, typename std::enable_if_t<IsWrappedClass<T>::value>> {
    using CType = std::shared_ptr<T>;
    using V8Type = v8::Local<v8::Object>;

    static bool IsValid(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        return Convert<T>::IsValid(isolate, value);
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid object");
        }
        return SharedPointerManager<std::remove_cv_t<T>>::UnwrapObject(isolate, value);
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return SharedPointerManager<std::remove_cv_t<T>>::FindObject(isolate, value);
    }
};



template<typename T>
struct Convert<T &> : Convert<T> {};

template<typename T>
struct Convert<const T &> : Convert<T> {};

template<typename T>
decltype(auto) FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
    return Convert<T>::FromV8(isolate, value);
}

template<typename T>
decltype(auto) ToV8(v8::Isolate *isolate, T &&t) {
    return Convert<T>::ToV8(isolate, std::forward<T>(t));
}

inline decltype(auto) ToV8(v8::Isolate *isolate, const char *c) {
    return Convert<std::string>::ToV8(isolate, std::string(c));
}

inline decltype(auto) ToV8(v8::Isolate *isolate, const char16_t *c) {
    return Convert<std::u16string>::ToV8(isolate, std::u16string(c));
}

inline decltype(auto) ToV8(v8::Isolate *isolate, const char32_t *c) {
    return Convert<std::u32string>::ToV8(isolate, std::u32string(c));
}

}

#endif //SANDWICH_V8B_CONVERT_HPP
