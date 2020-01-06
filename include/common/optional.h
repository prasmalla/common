/*  Copyright (C) 2014-2020 FastoGT. All right reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
        * Neither the name of FastoGT. nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <type_traits>
#include <utility>

#include <common/macros.h>

namespace common {
// Specification:
// http://en.cppreference.com/w/cpp/utility/optional/in_place_t
struct in_place_t {};
// Specification:
// http://en.cppreference.com/w/cpp/utility/optional/nullopt_t
struct nullopt_t {
  constexpr explicit nullopt_t(int) {}
};
// Specification:
// http://en.cppreference.com/w/cpp/utility/optional/in_place
constexpr in_place_t in_place = {};
// Specification:
// http://en.cppreference.com/w/cpp/utility/optional/nullopt
constexpr nullopt_t nullopt(0);
namespace internal {
template <typename T, bool = std::is_trivially_destructible<T>::value>
struct OptionalStorage {
  // Initializing |empty_| here instead of using default member initializing
  // to avoid errors in g++ 4.8.
  constexpr OptionalStorage() : empty_('\0') {}
  constexpr explicit OptionalStorage(const T& value) : is_null_(false), value_(value) {}
  // TODO(alshabalin): Can't use 'constexpr' with std::move until C++14.
  explicit OptionalStorage(T&& value) : is_null_(false), value_(std::move(value)) {}
  // TODO(alshabalin): Can't use 'constexpr' with std::forward until C++14.
  template <class... Args>
  explicit OptionalStorage(in_place_t, Args&&... args) : is_null_(false), value_(std::forward<Args>(args)...) {}
  // When T is not trivially destructible we must call its
  // destructor before deallocating its memory.
  ~OptionalStorage() {
    if (!is_null_)
      value_.~T();
  }
  bool is_null_ = true;
  union {
    // |empty_| exists so that the union will always be initialized, even when
    // it doesn't contain a value. Union members must be initialized for the
    // constructor to be 'constexpr'.
    char empty_;
    T value_;
  };
};
template <typename T>
struct OptionalStorage<T, true> {
  // Initializing |empty_| here instead of using default member initializing
  // to avoid errors in g++ 4.8.
  constexpr OptionalStorage() : empty_('\0') {}
  constexpr explicit OptionalStorage(const T& value) : is_null_(false), value_(value) {}
  // TODO(alshabalin): Can't use 'constexpr' with std::move until C++14.
  explicit OptionalStorage(T&& value) : is_null_(false), value_(std::move(value)) {}
  // TODO(alshabalin): Can't use 'constexpr' with std::forward until C++14.
  template <class... Args>
  explicit OptionalStorage(in_place_t, Args&&... args) : is_null_(false), value_(std::forward<Args>(args)...) {}
  // When T is trivially destructible (i.e. its destructor does nothing) there
  // is no need to call it. Explicitly defaulting the destructor means it's not
  // user-provided. Those two together make this destructor trivial.
  ~OptionalStorage() = default;
  bool is_null_ = true;
  union {
    // |empty_| exists so that the union will always be initialized, even when
    // it doesn't contain a value. Union members must be initialized for the
    // constructor to be 'constexpr'.
    char empty_;
    T value_;
  };
};
}  // namespace internal
// common::Optional is a Chromium version of the C++17 optional class:
// std::optional documentation:
// http://en.cppreference.com/w/cpp/utility/optional
// Chromium documentation:
// https://chromium.googlesource.com/chromium/src/+/master/docs/optional.md
//
// These are the differences between the specification and the implementation:
// - The constructor and emplace method using initializer_list are not
//   implemented because 'initializer_list' is banned from Chromium.
// - Constructors do not use 'constexpr' as it is a C++14 extension.
// - 'constexpr' might be missing in some places for reasons specified locally.
// - No exceptions are thrown, because they are banned from Chromium.
// - All the non-members are in the 'common' namespace instead of 'std'.
template <typename T>
class Optional {
 public:
  using value_type = T;
  constexpr Optional() {}
  constexpr Optional(nullopt_t) {}
  Optional(const Optional& other) {
    if (!other.storage_.is_null_)
      Init(other.value());
  }
  Optional(Optional&& other) {
    if (!other.storage_.is_null_)
      Init(std::move(other.value()));
  }
  constexpr Optional(const T& value) : storage_(value) {}
  // TODO(alshabalin): Can't use 'constexpr' with std::move until C++14.
  Optional(T&& value) : storage_(std::move(value)) {}
  // TODO(alshabalin): Can't use 'constexpr' with std::forward until C++14.
  template <class... Args>
  explicit Optional(in_place_t, Args&&... args) : storage_(in_place, std::forward<Args>(args)...) {}
  ~Optional() = default;
  Optional& operator=(nullopt_t) {
    FreeIfNeeded();
    return *this;
  }
  Optional& operator=(const Optional& other) {
    if (other.storage_.is_null_) {
      FreeIfNeeded();
      return *this;
    }
    InitOrAssign(other.value());
    return *this;
  }
  Optional& operator=(Optional&& other) {
    if (other.storage_.is_null_) {
      FreeIfNeeded();
      return *this;
    }
    InitOrAssign(std::move(other.value()));
    return *this;
  }
  template <class U>
  typename std::enable_if<std::is_same<std::decay<U>, T>::value, Optional&>::type operator=(U&& value) {
    InitOrAssign(std::forward<U>(value));
    return *this;
  }
  // TODO(mlamouri): can't use 'constexpr' with DCHECK.
  const T* operator->() const {
    DCHECK(!storage_.is_null_);
    return &value();
  }
  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T* operator->() {
    DCHECK(!storage_.is_null_);
    return &value();
  }
  constexpr const T& operator*() const& { return value(); }
  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T& operator*() & { return value(); }
  constexpr const T&& operator*() const&& { return std::move(value()); }
  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T&& operator*() && { return std::move(value()); }
  constexpr explicit operator bool() const { return !storage_.is_null_; }
  constexpr bool has_value() const { return !storage_.is_null_; }
  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T& value() & {
    DCHECK(!storage_.is_null_);
    return storage_.value_;
  }
  // TODO(mlamouri): can't use 'constexpr' with DCHECK.
  const T& value() const& {
    DCHECK(!storage_.is_null_);
    return storage_.value_;
  }
  // TODO(mlamouri): using 'constexpr' here breaks compiler that assume it was
  // meant to be 'constexpr const'.
  T&& value() && {
    DCHECK(!storage_.is_null_);
    return std::move(storage_.value_);
  }
  // TODO(mlamouri): can't use 'constexpr' with DCHECK.
  const T&& value() const&& {
    DCHECK(!storage_.is_null_);
    return std::move(storage_.value_);
  }
  template <class U>
  constexpr T value_or(U&& default_value) const& {
    // TODO(mlamouri): add the following assert when possible:
    // static_assert(std::is_copy_constructible<T>::value,
    //               "T must be copy constructible");
    COMPILE_ASSERT((std::is_convertible<U, T>::value), "U must be convertible to T");
    return storage_.is_null_ ? static_cast<T>(std::forward<U>(default_value)) : value();
  }
  template <class U>
  T value_or(U&& default_value) && {
    // TODO(mlamouri): add the following assert when possible:
    // static_assert(std::is_move_constructible<T>::value,
    //               "T must be move constructible");
    COMPILE_ASSERT((std::is_convertible<U, T>::value), "U must be convertible to T");
    return storage_.is_null_ ? static_cast<T>(std::forward<U>(default_value)) : std::move(value());
  }
  void swap(Optional& other) {
    if (storage_.is_null_ && other.storage_.is_null_)
      return;
    if (storage_.is_null_ != other.storage_.is_null_) {
      if (storage_.is_null_) {
        Init(std::move(other.storage_.value_));
        other.FreeIfNeeded();
      } else {
        other.Init(std::move(storage_.value_));
        FreeIfNeeded();
      }
      return;
    }
    DCHECK(!storage_.is_null_ && !other.storage_.is_null_);
    using std::swap;
    swap(**this, *other);
  }
  void reset() { FreeIfNeeded(); }
  template <class... Args>
  void emplace(Args&&... args) {
    FreeIfNeeded();
    Init(std::forward<Args>(args)...);
  }

 private:
  void Init(const T& value) {
    DCHECK(storage_.is_null_);
    new (&storage_.value_) T(value);
    storage_.is_null_ = false;
  }
  void Init(T&& value) {
    DCHECK(storage_.is_null_);
    new (&storage_.value_) T(std::move(value));
    storage_.is_null_ = false;
  }
  template <class... Args>
  void Init(Args&&... args) {
    DCHECK(storage_.is_null_);
    new (&storage_.value_) T(std::forward<Args>(args)...);
    storage_.is_null_ = false;
  }
  void InitOrAssign(const T& value) {
    if (storage_.is_null_)
      Init(value);
    else
      storage_.value_ = value;
  }
  void InitOrAssign(T&& value) {
    if (storage_.is_null_)
      Init(std::move(value));
    else
      storage_.value_ = std::move(value);
  }
  void FreeIfNeeded() {
    if (storage_.is_null_)
      return;
    storage_.value_.~T();
    storage_.is_null_ = true;
  }
  internal::OptionalStorage<T> storage_;
};
template <class T>
constexpr bool operator==(const Optional<T>& lhs, const Optional<T>& rhs) {
  return !!lhs != !!rhs ? false : lhs == nullopt || (*lhs == *rhs);
}
template <class T>
constexpr bool operator!=(const Optional<T>& lhs, const Optional<T>& rhs) {
  return !(lhs == rhs);
}
template <class T>
constexpr bool operator<(const Optional<T>& lhs, const Optional<T>& rhs) {
  return rhs == nullopt ? false : (lhs == nullopt ? true : *lhs < *rhs);
}
template <class T>
constexpr bool operator<=(const Optional<T>& lhs, const Optional<T>& rhs) {
  return !(rhs < lhs);
}
template <class T>
constexpr bool operator>(const Optional<T>& lhs, const Optional<T>& rhs) {
  return rhs < lhs;
}
template <class T>
constexpr bool operator>=(const Optional<T>& lhs, const Optional<T>& rhs) {
  return !(lhs < rhs);
}
template <class T>
constexpr bool operator==(const Optional<T>& opt, nullopt_t) {
  return !opt;
}
template <class T>
constexpr bool operator==(nullopt_t, const Optional<T>& opt) {
  return !opt;
}
template <class T>
constexpr bool operator!=(const Optional<T>& opt, nullopt_t) {
  return !!opt;
}
template <class T>
constexpr bool operator!=(nullopt_t, const Optional<T>& opt) {
  return !!opt;
}
template <class T>
constexpr bool operator<(const Optional<T>&, nullopt_t) {
  return false;
}
template <class T>
constexpr bool operator<(nullopt_t, const Optional<T>& opt) {
  return !!opt;
}
template <class T>
constexpr bool operator<=(const Optional<T>& opt, nullopt_t) {
  return !opt;
}
template <class T>
constexpr bool operator<=(nullopt_t, const Optional<T>&) {
  return true;
}
template <class T>
constexpr bool operator>(const Optional<T>& opt, nullopt_t) {
  return !!opt;
}
template <class T>
constexpr bool operator>(nullopt_t, const Optional<T>&) {
  return false;
}
template <class T>
constexpr bool operator>=(const Optional<T>&, nullopt_t) {
  return true;
}
template <class T>
constexpr bool operator>=(nullopt_t, const Optional<T>& opt) {
  return !opt;
}
template <class T>
constexpr bool operator==(const Optional<T>& opt, const T& value) {
  return opt != nullopt ? *opt == value : false;
}
template <class T>
constexpr bool operator==(const T& value, const Optional<T>& opt) {
  return opt == value;
}
template <class T>
constexpr bool operator!=(const Optional<T>& opt, const T& value) {
  return !(opt == value);
}
template <class T>
constexpr bool operator!=(const T& value, const Optional<T>& opt) {
  return !(opt == value);
}
template <class T>
constexpr bool operator<(const Optional<T>& opt, const T& value) {
  return opt != nullopt ? *opt < value : true;
}
template <class T>
constexpr bool operator<(const T& value, const Optional<T>& opt) {
  return opt != nullopt ? value < *opt : false;
}
template <class T>
constexpr bool operator<=(const Optional<T>& opt, const T& value) {
  return !(opt > value);
}
template <class T>
constexpr bool operator<=(const T& value, const Optional<T>& opt) {
  return !(value > opt);
}
template <class T>
constexpr bool operator>(const Optional<T>& opt, const T& value) {
  return value < opt;
}
template <class T>
constexpr bool operator>(const T& value, const Optional<T>& opt) {
  return opt < value;
}
template <class T>
constexpr bool operator>=(const Optional<T>& opt, const T& value) {
  return !(opt < value);
}
template <class T>
constexpr bool operator>=(const T& value, const Optional<T>& opt) {
  return !(value < opt);
}
template <class T>
constexpr Optional<typename std::decay<T>::type> make_optional(T&& value) {
  return Optional<typename std::decay<T>::type>(std::forward<T>(value));
}
template <class T>
void swap(Optional<T>& lhs, Optional<T>& rhs) {
  lhs.swap(rhs);
}
}  // namespace common

namespace std {
template <class T>
struct hash<common::Optional<T>> {
  size_t operator()(const common::Optional<T>& opt) const { return opt == common::nullopt ? 0 : std::hash<T>()(*opt); }
};
}  // namespace std
