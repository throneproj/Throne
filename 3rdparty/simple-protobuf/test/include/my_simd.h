#pragma once

//- no simd, use fixed-size-array instead
#include <array>
template <class Tp, int Np> using my_fixed_size_simd = std::array<Tp, Np>;
