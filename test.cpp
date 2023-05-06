#include "slice.h"
#include "slice_buffer.h"
#include <iostream>
#include <gtest/gtest.h>

Slice MakeSlice(size_t len) {
  Slice slice(len);
  auto data = slice.MutableData();
  for (size_t i = 0; i < len; ++i) {
    data[i] = i & 0xff;
  }
  return slice;
}

TEST(Slice, init) {
  uint8_t data1[] = {1, 2, 3, 4, 5, 6, 7, 8};
  Slice test(data1, sizeof(data1));
  EXPECT_EQ(test.GetLength(), sizeof(data1));

  Slice test100 = MakeSlice(100);
  EXPECT_EQ(test100.GetLength(), 100);

  Slice ref(test100);

  EXPECT_EQ(test100, ref);
}

TEST(Slice, sub) {
  Slice test = MakeSlice(8);
  auto sub1 = test.Sub(1, 3);
  auto sub2 = test.Sub(1, 3);

  EXPECT_EQ(sub1, sub2);

  EXPECT_THROW({
    test.Sub(0,10);
  }, std::exception);
}

TEST(Slice, merge) {
  {
    Slice test = MakeSlice(8);
    auto sub1 = test.Sub(1, 3);
    auto sub2 = test.Sub(1, 3);
    EXPECT_EQ(sub1, sub2);

    sub1.Merge(sub2);
    sub1.Dump();
    EXPECT_EQ(sub1.GetLength(), 4);
  }


  {
    Slice test100 = MakeSlice(100);
    Slice sub1 = test100.Sub(0, 10);
    Slice sub2 = test100.Sub(10, 20);

    sub1.Merge(sub2);
    EXPECT_EQ(sub1.GetLength(), 20);

  }

  SliceWrapper<int> num(112);

  EXPECT_EQ(*num, 112);
}

TEST(SliceBuffer, add) {
  SliceBuffer buffer;
  buffer.Add(MakeSlice(8));

  buffer.Add(MakeSlice(18));

  buffer.Add(MakeSlice(32));
  buffer.Add(MakeSlice(5));
  buffer.Add(MakeSlice(35));

  Slice slice = buffer.PopFront();
 
  buffer.UnPopFront(slice);

  int ss = 0;
}
