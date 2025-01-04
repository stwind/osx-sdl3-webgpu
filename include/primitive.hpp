#pragma once

#include <vector>

namespace prim {
  void gnomon(std::vector<float>& V, float s = 1.) {
    V.resize(36);
    V.assign({
      0, 0, 0, 1, 0, 0,
      s, 0, 0, 1, 0, 0, // x
      0, 0, 0, 0, 1, 0,
      0, s, 0, 0, 1, 0, // y
      0, 0, 0, 0, 0, 1,
      0, 0, s, 0, 0, 1, // z
      });
  };

  void cube(std::vector<float>& V, std::vector<uint16_t>& F, float s = 1.) {
    V.resize(144);
    V.assign({
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
      });

    F.resize(36);
    F.assign({
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
      });
  };
}
