#ifndef __SLICE_BUFFER_H__
#define __SLICE_BUFFER_H__

#include "slice.h"
#include <array>

class SliceBuffer {
public:
  static constexpr size_t kInlinedCount = 8;
  SliceBuffer();
  ~SliceBuffer();

  SliceBuffer(const SliceBuffer&) = delete;
  SliceBuffer(SliceBuffer&&) = delete;
  SliceBuffer& operator=(const SliceBuffer&) = delete;
  SliceBuffer& operator=(SliceBuffer&&) = delete;


  /// @brief add slice to tail.
  /// @param slice 
  void Add(Slice const& slice);

  /// @brief add slice buffer to tail.
  /// @param slice buffer 
  void Add(SliceBuffer const& slice_buffer);


  /// @brief pop front slice.
  /// @return slice
  Slice PopFront();

  /// @brief cancel pop front slice.
  /// @param slice
  bool UnPopFront(Slice const& slice);

  /// @brief pop back slice.
  /// @return slice
  Slice PopBack();

  /// @brief get slice count.
  /// @return count
  inline size_t GetCount() const {
    return count_;
  }

  /// @brief get total bytes.
  /// @return bytes
  inline size_t GetLength() const {
    return length_;
  }

  /// @brief get slice at index.
  /// @param index 
  /// @return slice
  Slice GetAt(size_t index) const;

  /// @brief is empty.
  inline bool IsEmpty() const {
    return count_ == 0;
  }

private:
  void maybe_embiggen();
  void do_embiggen(size_t slice_count, size_t slice_offset);

  Slice *base_slices_;
  Slice *slices_;
  Slice inlined_slices_[kInlinedCount];

  ///@brief the number of slices in the array
  size_t count_;

  ///@brief the number of slices allocated in the array.
  size_t capacity_;

  ///@brief the combined length of all slices in the array
  size_t length_;
};

#endif // __SLICE_BUFFER_H__