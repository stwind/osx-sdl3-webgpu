#pragma once

#include <vector>

namespace prim {
  std::vector<float> gnomon(float s = 1.) {
    std::vector<float> data{
      0, 0, 0, 1, 0, 0,
      s, 0, 0, 1, 0, 0, // x
      0, 0, 0, 0, 1, 0,
      0, s, 0, 0, 1, 0, // y
      0, 0, 0, 0, 0, 1,
      0, 0, s, 0, 0, 1, // z
    };
    return data;
  };

  std::tuple<std::vector<float>, std::vector<uint16_t>> cube(float s = 1.) {
    std::vector<float> vertices{
      s, s, -s, 1, 0, 0,
      s, s, s, 1, 0, 0,
      s, -s, s, 1, 0, 0,
      s, -s, -s, 1, 0, 0,
      -s, s, s, -1, 0, 0,
      -s, s, -s, -1, 0, 0,
      -s, -s, -s, -1, 0, 0,
      -s, -s, s, -1, 0, 0,
      -s, s, s, 0, 1, 0,
      s, s, s, 0, 1, 0,
      s, s, -s, 0, 1, 0,
      -s, s, -s, 0, 1, 0,
      -s, -s, -s, 0, -1, 0,
      s, -s, -s, 0, -1, 0,
      s, -s, s, 0, -1, 0,
      -s, -s, s, 0, -1, 0,
      s, s, s, 0, 0, 1,
      -s, s, s, 0, 0, 1,
      -s, -s, s, 0, 0, 1,
      s, -s, s, 0, 0, 1,
      -s, s, -s, 0, 0, -1,
      s, s, -s, 0, 0, -1,
      s, -s, -s, 0, 0, -1,
      -s, -s, -s, 0, 0, -1,
    };

    std::vector<uint16_t> indices{
      0, 1, 2,
      0, 2, 3,
      4, 5, 6,
      4, 6, 7,
      8, 9, 10,
      8, 10, 11,
      12, 13, 14,
      12, 14, 15,
      16, 17, 18,
      16, 18, 19,
      20, 21, 22,
      20, 22, 23
    };

    return { vertices, indices };
  };
}
