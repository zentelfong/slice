#include "slice_buffer.h"
#include <string.h>
#include <utility>

// grow a buffer; requires GRPC_SLICE_BUFFER_INLINE_ELEMENTS > 1
#define GROW(x) (3 * (x) / 2)

SliceBuffer::SliceBuffer()
  :base_slices_(inlined_slices_), slices_(inlined_slices_), 
  count_(0), length_(0), capacity_(kInlinedCount) {

}


SliceBuffer::~SliceBuffer() {
  if (base_slices_ != inlined_slices_) {
    delete[] base_slices_;
  }
}

void SliceBuffer::maybe_embiggen() {
  if (count_ == 0) {
    slices_ = base_slices_;
    return;
  }

  // How far away from sb->base_slices is sb->slices pointer
  size_t slice_offset = static_cast<size_t>(slices_ - base_slices_);
  size_t slice_count = count_ + slice_offset;
  if (slice_count == capacity_) {
    do_embiggen(slice_count, slice_offset);
  }
}

void SliceBuffer::do_embiggen(size_t slice_count, size_t slice_offset) {
  if (slice_offset != 0) {
    // Make room by moving elements if there's still space unused
    for (size_t i = 0; i < count_; ++i) {
      base_slices_[i] = slices_[i];
    }
    slices_ = base_slices_;
  } else {
    // Allocate more memory if no more space is available
    const size_t new_capacity = GROW(capacity_);
    capacity_ = new_capacity;
    if (base_slices_ == inlined_slices_) {
      base_slices_ = new Slice[new_capacity];
      for (size_t i = 0; i < slice_count; ++i) {
        base_slices_[i] = inlined_slices_[i];
      }
    } else {
      auto old_slices = base_slices_;
      base_slices_ = new Slice[new_capacity];
      for (size_t i = 0; i < slice_count; ++i) {
        base_slices_[i] = old_slices[i];
      }
      delete[] old_slices;
    }
    slices_ = base_slices_ + slice_offset;
  }
}


void SliceBuffer::Add(Slice const& slice) {
  size_t n = count_;
  Slice* back = nullptr;
  if (n != 0) {
    back = &slices_[n - 1];
  }

  if (back && back->Merge(slice)) {
    length_ += slice.GetLength();
    return;
  }

  maybe_embiggen();
  size_t out = count_;
  slices_[out] = slice;
  length_ += slice.GetLength();
  count_ = out + 1;
}

Slice SliceBuffer::PopFront() {
  if (count_ == 0) {
    throw std::logic_error("no slice");
  }

  Slice slice = slices_[0];
  slices_++;
  count_--;
  length_ -= slice.GetLength();
  return slice;
}

bool SliceBuffer::UnPopFront(Slice const& slice) {
  if (slices_ == base_slices_) {
    return false;
  }

  slices_--;
  slices_[0] = slice;
  count_++;
  length_ += slice.GetLength();
  return true;
}

Slice SliceBuffer::PopBack() {
  if (count_ == 0) {
    throw std::logic_error("no slice");
  }

  Slice slice = slices_[count_ - 1];
  count_--;
  length_ -= slice.GetLength();
  return slice;
}

Slice SliceBuffer::GetAt(size_t index) const {
  if (index >= count_) {
    throw std::invalid_argument("invalied index");
  }
  return slices_[index];
}


