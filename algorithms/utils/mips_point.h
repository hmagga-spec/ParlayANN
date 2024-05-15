// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <algorithm>
#include <iostream>

#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/internal/file_map.h"
#include "../bench/parse_command_line.h"
#include "NSGDist.h"

#include "../bench/parse_command_line.h"
#include "types.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


  float mips_distance(const uint8_t *p, const uint8_t *q, unsigned d) {
    int result = 0;
    for (int i = 0; i < d; i++) {
      result += ((int32_t)q[i]) * ((int32_t)p[i]);
    }
    return -((float)result);
  }

  float mips_distance(const int8_t *p, const int8_t *q, unsigned d) {
    int result = 0;
    for (int i = 0; i < d; i++) {
      result += ((int32_t)q[i]) * ((int32_t)p[i]);
    }
    return -((float)result);
  }

  float mips_distance(const float *p, const float *q, unsigned d) {
    float result = 0;
    for (int i = 0; i < d; i++) {
      result += (q[i]) * (p[i]);
    }
    return -result;
  }

template<typename T>
struct Mips_Point {
  using distanceType = float; 
  //template<typename C, typename range> friend struct Quantized_Mips_Point;

  struct parameters {
    int dims;
    parameters() : dims(0) {}
    parameters(int dims) : dims(dims) {}
  };

  static distanceType d_min() {return -std::numeric_limits<float>::max();}
  static bool is_metric() {return false;}
  T operator [](long i) const {return *(values + i);}

  float distance(const Mips_Point<T>& x) const {
    return mips_distance(this->values, x.values, params.dims);
  }

  void prefetch() const {
    int l = (params.dims * sizeof(T) - 1)/64 + 1;
    for (int i=0; i < l; i++)
      __builtin_prefetch((char*) values + i* 64);
  }

  long id() const {return id_;}

  Mips_Point() : values(nullptr), id_(-1), params(0) {}

  Mips_Point(T* values, long id, parameters params)
    : values(values), id_(id), params(params) {}

  bool operator==(const Mips_Point<T>& q) const {
    for (int i = 0; i < params.dims; i++) {
      if (values[i] != q.values[i]) {
        return false;
      }
    }
    return true;
  }

  bool same_as(const Mips_Point<T>& q){
    return values == q.values;
  }

  template <typename Point>
  static void translate_point(T* values, const Point& p, const parameters& params) {
    for (int j = 0; j < params.dims; j++) values[j] = (T) p[j];
  }
  
  template <typename PR>
  static parameters generate_parameters(const PR& pr) {
    return parameters(pr.dimension());}

private:
  T* values;
  long id_;
  parameters params;
};

template<typename T, int range=(1 << sizeof(T)*8) - 1>
struct Quantized_Mips_Point{
  using distanceType = float; 
  
  struct parameters {
    float slope;
    int32_t offset;
    int dims;
    parameters(int dims) : slope(0), offset(0), dims(dims) {}
    parameters(float min_val, float max_val, int dims)
      : slope(range / (max_val - min_val)),
        offset((int32_t) round(min_val * slope)),
        dims(dims) {}
  };

  static distanceType d_min() {return -std::numeric_limits<float>::max();}
  static bool is_metric() {return false;}
  
  //T& operator [] (long j) const {if (j >= d) abort(); return *(values+j);}
  T operator [] (long j) const {return *(values+j);}

  float distance(uint8_t* p, uint8_t* q, int32_t o) const {
    int32_t result = 0;
    for (int i = 0; i < params.dims; i++){
      result += (p[i] + o) * (q[i] + o);
    }
    return - (float) result;
  }

  float distance(uint16_t* p, uint16_t* q, int32_t o) const {
    int64_t result = 0;
    for (int i = 0; i < params.dims; i++){
      result += (p[i] + o) * (q[i] + o);
    }
    return - (float) result;
  }

  float distance(const Quantized_Mips_Point &x) const {
    return distance(this->values, x.values, params.offset);
  }

  void prefetch() const {
    int l = (params.dims * sizeof(T) - 1)/64 + 1;
    for (int i=0; i < l; i++)
      __builtin_prefetch((char*) values + i * 64);
  }

  bool same_as(const Quantized_Mips_Point& q){
    return values == q.values;
  }

  long id() const {return id_;}

  Quantized_Mips_Point(T* values, long id, parameters p)
    : values(values), id_(id), params(p)
  {}

  bool operator==(const Quantized_Mips_Point &q) const {
    for (int i = 0; i < params.dims; i++) {
      if (values[i] != q.values[i]) {
        return false;
      }
    }
    return true;
  }

  template <typename Point>
  static void translate_point(T* values, const Point& p, const parameters& params) {
    float slope = params.slope;
    int32_t offset = params.offset;
    for (int j = 0; j < params.dims; j++) {
      auto x = p[j];
      values[j] = (T) (std::round(x * slope)) - offset;
    }
  }

  template <typename PR>
  static parameters generate_parameters(const PR& pr) {
    long n = pr.size();
    int dims = pr.dimension();
    parlay::sequence<typename PR::T> mins(n);
    parlay::sequence<typename PR::T> maxs(n);
    parlay::parallel_for(0, n, [&] (long i) {
      mins[i] = 0.0;
      maxs[i] = 0.0;
      for (int j = 0; j < dims; j++) {
        mins[i]= std::min(mins[i], pr[i][j]);
        maxs[i]= std::max(maxs[i], pr[i][j]);}});
    float min_val = *parlay::min_element(mins);
    float max_val = *parlay::max_element(maxs);
    return parameters(min_val, max_val, dims);
  }

private:
  T* values;
  long id_;
  parameters params;
};
