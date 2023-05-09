#ifndef __SLICE_H__
#define __SLICE_H__

#include <string>
#include <cstddef>

class SliceRefcount;

class Slice {
public:
  static constexpr size_t kSliceInlinedDataSize 
    = sizeof(size_t) + sizeof(uint8_t *) + sizeof(void*) - 1;

  Slice();
  explicit Slice(size_t length);
  Slice(SliceRefcount *refcount, uint8_t *bytes, size_t length);
  Slice(uint8_t const*bytes, size_t length);

  Slice(Slice const& slice);
  Slice(Slice && slice);
  
  ~Slice();

  Slice &operator=(Slice const& slice);
  Slice &operator=(Slice && slice);

  bool operator==(Slice const& slice) const;

  /// @brief get data
  /// @return data
  inline uint8_t const* GetData() const {
    return refcount_ ? data_.refcounted.bytes : data_.inlined.bytes;
  }

  /// @brief get mutable data
  /// @return data
  inline uint8_t* MutableData() {
    return refcount_ ? data_.refcounted.bytes : data_.inlined.bytes;
  }

  /// @brief get length
  /// @return length
  inline size_t GetLength() const {
    return refcount_ ? data_.refcounted.length : data_.inlined.length;
  }

  inline std::string GetString() const {
    return std::string(reinterpret_cast<char const*>(GetData()), GetLength());
  }

  /// @brief is empty.
  inline bool IsEmpty() const {
    return GetLength() == 0;
  }

  /// @brief clone a slice
  /// @return slice
  Slice Clone() const;

  void Clear();

  /// @brief sub
  /// @param begin 
  /// @param end 
  /// @return slice
  Slice Sub(size_t begin, size_t end) const;

  void Dump() const;
  
  /// @brief merge with another slice
  /// @param slice
  bool Merge(Slice const& slice);

  static Slice FromStaticBuffer(void const* s, size_t len);
  static Slice FromStaticString(char const* s);
  static Slice FromString(std::string const& s);
  static Slice FromString(std::string && s);

private:
  void init(size_t length);

  SliceRefcount *refcount_{nullptr};

  union SliceData {
    struct {
      size_t   length;
      uint8_t* bytes;
    } refcounted;
    struct {
      uint8_t length;
      uint8_t bytes[kSliceInlinedDataSize];
    } inlined;
  } data_;
};


/// @brief SliceWrapper used to wrap a struct to Slice
/// @tparam T 
template<typename T>
class SliceWrapper : public Slice {
public:
  template<typename... Args>
  SliceWrapper(Args&&... args)
    :Slice(sizeof(T)) {
      new(MutableData())T(std::forward<Args>(args)...);
  }

  operator T* () {
    return reinterpret_cast<T*>(MutableData());
  }

  T* operator->() {
    return reinterpret_cast<T*>(MutableData());
  }
};

#endif // __SLICE_H__
