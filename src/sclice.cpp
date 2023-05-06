#include "slice.h"
#include <atomic>
#include <new>
#include <stdexcept>
#include <string.h>

class SliceRefcount {
public:
  typedef void (*DestroyerFn)(SliceRefcount *);

  SliceRefcount() = default;

  static SliceRefcount *NoopRefcount() {
    static SliceRefcount noop_ref;
    return &noop_ref;
  }

  // Regular constructor for grpc_slice_refcount.
  //
  // Parameters:
  //  1. DestroyerFn destroyer_fn
  //  Called when the refcount goes to 0, with 'this' as parameter.
  explicit SliceRefcount(DestroyerFn destroyer_fn)
      : destroyer_fn_(destroyer_fn) {}

  void Ref() {
    if (destroyer_fn_) {
      ref_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  void Unref() {
    if (destroyer_fn_) {
      auto prev_refs = ref_.fetch_sub(1, std::memory_order_acq_rel);
      if (prev_refs == 1) {
        destroyer_fn_(this);
      }
    }
  }

  // Is this the only instance?
  // For this to be useful the caller needs to ensure that if this is the only
  // instance, no other instance could be created during this call.
  bool IsUnique() const { return ref_.load(std::memory_order_relaxed) == 1; }

private:
  std::atomic<size_t> ref_{1};
  DestroyerFn destroyer_fn_ = nullptr;
};

Slice::Slice() { data_.inlined.length = 0; }

Slice::Slice(size_t length) { init(length); }

Slice::Slice(SliceRefcount *refcount, uint8_t *bytes, size_t length) {
  refcount->Ref();
  refcount_ = refcount;
  data_.refcounted.bytes = bytes;
  data_.refcounted.length = length;
}

Slice::Slice(uint8_t const *bytes, size_t length) {
  init(length);
  memcpy(MutableData(), bytes, length);
}

Slice::Slice(Slice const &slice) {
  if (slice.refcount_) {
    refcount_ = slice.refcount_;
    refcount_->Ref();
    data_.refcounted.bytes = slice.data_.refcounted.bytes;
    data_.refcounted.length = slice.data_.refcounted.length;
  } else {
    data_.inlined = slice.data_.inlined;
  }
}

Slice::Slice(Slice &&slice) {
  if (slice.refcount_) {
    refcount_ = slice.refcount_;
    data_.refcounted = slice.data_.refcounted;

    slice.refcount_ = nullptr;
    slice.data_.inlined.length = 0;
  } else {
    data_.inlined = slice.data_.inlined;
  }
}

Slice::~Slice() {
  if (refcount_) {
    refcount_->Unref();
  }
}

void Slice::init(size_t length) {
  if (length <= kSliceInlinedDataSize) {
    data_.inlined.length = length;
  } else {
    uint8_t *memory = new uint8_t[sizeof(SliceRefcount) + length];
    refcount_ = new (memory) SliceRefcount(
        [](SliceRefcount *p) { delete[] reinterpret_cast<uint8_t *>(p); });
    data_.refcounted.bytes = memory + sizeof(SliceRefcount);
    data_.refcounted.length = length;
  }
}

Slice &Slice::operator=(Slice const &slice) {

  if (refcount_ == slice.refcount_) {
    data_.refcounted = slice.data_.refcounted;
  } else {
    if (refcount_) {
      refcount_->Unref();
      refcount_ = nullptr;
    }

    if (slice.refcount_) {
      refcount_ = slice.refcount_;
      refcount_->Ref();
      data_.refcounted = slice.data_.refcounted;
    } else {
      data_.inlined = slice.data_.inlined;
    }
  }

  return *this;
}

Slice &Slice::operator=(Slice &&slice) {
  if (refcount_) {
    refcount_->Unref();
    refcount_ = nullptr;
  }

  if (slice.refcount_) {
    refcount_ = slice.refcount_;
    data_.refcounted = slice.data_.refcounted;

    slice.refcount_ = nullptr;
    slice.data_.inlined.length = 0;
  } else {
    data_.inlined = slice.data_.inlined;
  }
  return *this;
}

bool Slice::operator==(Slice const &slice) const {
  if (GetLength() != slice.GetLength()) {
    return false;
  }

  if (refcount_ && refcount_ == slice.refcount_) {
    return data_.refcounted.bytes == slice.data_.refcounted.bytes;
  } else {
    return 0 == memcmp(GetData(), slice.GetData(), GetLength());
  }
}

Slice Slice::Clone() const {
  Slice slice(GetLength());
  memcpy(slice.MutableData(), GetData(), GetLength());
  return slice;
}

Slice Slice::Sub(size_t begin, size_t end) const {
  if (begin > end) {
    throw std::invalid_argument("invalied param");
  }

  if (GetLength() < end) {
    throw std::invalid_argument("out of range");
  }

  Slice subset;

  if (end - begin <= sizeof(subset.data_.inlined.bytes)) {
    subset.refcount_ = nullptr;
    subset.data_.inlined.length = static_cast<uint8_t>(end - begin);
    memcpy(subset.data_.inlined.bytes, GetData() + begin, end - begin);
  } else {
    if (refcount_ != nullptr) {
      // Build the result
      subset.refcount_ = refcount_;
      // Point into the source array
      subset.data_.refcounted.bytes = data_.refcounted.bytes + begin;
      subset.data_.refcounted.length = end - begin;
    } else {
      // Enforce preconditions
      subset.refcount_ = nullptr;
      subset.data_.inlined.length = static_cast<uint8_t>(end - begin);
      memcpy(subset.data_.inlined.bytes, data_.inlined.bytes + begin,
             end - begin);
    }
  }
  return subset;
}

Slice Slice::FromStaticBuffer(void const *s, size_t len) {
  Slice slice(SliceRefcount::NoopRefcount(),
              const_cast<uint8_t *>(static_cast<uint8_t const *>(s)), len);
  return slice;
}

Slice Slice::FromStaticString(char const *s) {
  Slice slice(SliceRefcount::NoopRefcount(),
              const_cast<uint8_t *>(reinterpret_cast<uint8_t const *>(s)),
              strlen(s));
  return slice;
}

Slice Slice::FromString(std::string const &s) {
  return Slice(reinterpret_cast<uint8_t const *>(s.data()), s.length());
}

class MovedCppStringSliceRefCount : public SliceRefcount {
public:
  explicit MovedCppStringSliceRefCount(std::string &&str)
      : SliceRefcount(Destroy), str_(std::move(str)) {}

  uint8_t *data() {
    return reinterpret_cast<uint8_t *>(const_cast<char *>(str_.data()));
  }

  size_t size() const { return str_.size(); }

private:
  static void Destroy(SliceRefcount *arg) {
    delete static_cast<MovedCppStringSliceRefCount *>(arg);
  }

  std::string str_;
};

Slice Slice::FromString(std::string &&str) {
  Slice slice;
  if (str.size() <= sizeof(slice.data_.inlined.bytes)) {
    slice.refcount_ = nullptr;
    slice.data_.inlined.length = str.size();
    memcpy(slice.MutableData(), str.data(), str.size());
  } else {
    auto *refcount = new MovedCppStringSliceRefCount(std::move(str));
    slice.data_.refcounted.bytes = refcount->data();
    slice.data_.refcounted.length = refcount->size();
    slice.refcount_ = refcount;
  }
  return slice;
}

void Slice::Dump() const {
  auto length = GetLength();
  auto data = GetData();

  printf("Slice:%u", (uint32_t)length);
  int m = 0;
  for (int i = 0; i < length; i++) {
    if (i % 16 == 0) {
      printf("\n  0x%04x: ", (8 * m));
      m++;
    }
    printf("%02X ", data[i]);
  }
  printf("\n\n");
}

bool Slice::Merge(Slice const &slice) {
  if (refcount_ && refcount_ == slice.refcount_ &&
      data_.refcounted.bytes + data_.refcounted.length == slice.GetData()) {
    data_.refcounted.length += slice.data_.refcounted.length;
    return true;
  }

  if (!refcount_ && !slice.refcount_) {
    if (data_.inlined.length + slice.data_.inlined.length <=
        kSliceInlinedDataSize) {
      memcpy(data_.inlined.bytes + data_.inlined.length,
             slice.data_.inlined.bytes, slice.data_.inlined.length);
      data_.inlined.length += slice.data_.inlined.length;
    } else {
      auto length = data_.inlined.length;

      init(length + slice.data_.inlined.length);

      memcpy(MutableData(), data_.inlined.bytes, length);
      memcpy(MutableData() + length, slice.data_.inlined.bytes, slice.data_.inlined.length);
    }
    return true;
  }
  return false;
}
