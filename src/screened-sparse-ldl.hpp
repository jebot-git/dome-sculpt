// SPDX-License-Identifier: LGPL-2.1-or-later
// Sparse symbolic/numeric LDL structure adapted from Timothy A. Davis LDL,
// Copyright (c)2005-2022 Timothy A. Davis. All Rights Reserved.
// Native bounded C++ staging, exact minimum-degree ordering, cancellation and
// positive preconditioner pivots: Dome Sculpt contributors2026.
// Pinned source/provenance: docs/validation/BIHARMONIC-LDL-REFERENCE.json.
// License and complete LGPL text: third_party/ldl/.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>
namespace dome::detail {
// Exact fixed screened graph factor. The original matrix, positivity and
// partition certificates remain acceptance authority. No Q product or pivot
// regularization is used. Ordering/fill/numeric work are bounded before use.
class ScreenedSparseLDL {
  std::vector<size_t> ap_, lp_;
  std::vector<uint32_t> ai_, li_, permutation_, inverse_, lnz_, pattern_;
  std::vector<int32_t> parent_, flag_;
  std::vector<double> ax_, lx_, d_, y_, scratch_, diagonal_;
  std::vector<uint8_t> cached_;
  std::vector<size_t> free_ap_;
  std::vector<uint32_t> free_ai_, free_index_, free_old_;
  std::vector<double> free_ax_;
  size_t free_n_ = 0;
  bool valid_ = false;
  template <class Budget>
  void factor(std::span<const uint8_t> status, Budget &budget) {
    const size_t n = d_.size();
    valid_ = false;
    free_n_ = 0;
    budget.step(n);
    for (uint32_t old = 0; old < n; ++old) {
      if (status[permutation_[old]])
        free_index_[old] = UINT32_MAX;
      else {
        free_index_[old] = uint32_t(free_n_);
        free_old_[free_n_++] = old;
      }
    }
    if (!free_n_)
      throw std::logic_error("Sparse LDL requested with no free variables");
    const size_t nf = free_n_;
    free_ap_[0] = 0;
    size_t nz = 0;
    for (size_t k = 0; k < nf; ++k) {
      budget.step(1);
      auto old = free_old_[k];
      for (size_t at = ap_[old]; at < ap_[old + 1]; ++at) {
        budget.step(1);
        auto row = free_index_[ai_[at]];
        if (row != UINT32_MAX) {
          free_ai_[nz] = row;
          free_ax_[nz++] = ax_[at];
        }
      }
      free_ap_[k + 1] = nz;
    }
    // Every free elimination path is also an earlier-vertex path in the
    // global graph. Restricting its ordering cannot introduce outside fill.
    for (uint32_t k = 0; k < nf; ++k) {
      budget.stop();
      budget.step(4);
      parent_[k] = -1;
      flag_[k] = int32_t(k);
      lnz_[k] = 0;
      for (size_t at = free_ap_[k]; at < free_ap_[k + 1]; ++at) {
        budget.step(1);
        auto i = free_ai_[at];
        if (i < k)
          while (flag_[i] != int32_t(k)) {
            budget.step(4);
            if (parent_[i] < 0)
              parent_[i] = int32_t(k);
            ++lnz_[i];
            flag_[i] = int32_t(k);
            i = uint32_t(parent_[i]);
          }
      }
    }
    lp_[0] = 0;
    predicted_factor_work = n + 10 * nf + 4 * nz;
    for (size_t i = 0; i < nf; ++i) {
      budget.step(1);
      lp_[i + 1] = lp_[i] + lnz_[i];
      predicted_factor_work += size_t(lnz_[i]) * (lnz_[i] + 12);
    }
    if (lp_[nf] > factor_nonzeros)
      throw std::logic_error(
          "Free principal factor exceeded global symbolic fill bound");
    if (predicted_factor_work > budget.maximum - budget.used)
      throw std::length_error("Free sparse LDL numeric work exceeds remaining "
                              "budget before factorization");
    size_t changed = 0;
    const auto before = budget.used;
    budget.step(nf);
    std::fill(y_.begin(), y_.begin() + nf, 0);
    for (uint32_t k = 0; k < nf; ++k) {
      budget.stop();
      budget.step(8);
      y_[k] = 0;
      size_t top = nf;
      flag_[k] = int32_t(k);
      lnz_[k] = 0;
      for (size_t at = free_ap_[k]; at < free_ap_[k + 1]; ++at) {
        budget.step(4);
        auto i = free_ai_[at];
        y_[i] += free_ax_[at];
        size_t length = 0;
        while (flag_[i] != int32_t(k)) {
          budget.step(4);
          if (length >= nf || parent_[i] < 0)
            throw std::logic_error("Invalid free LDL elimination path");
          pattern_[length++] = i;
          flag_[i] = int32_t(k);
          i = uint32_t(parent_[i]);
        }
        while (length) {
          budget.step(1);
          if (!top)
            throw std::logic_error("Free LDL pattern stack overflow");
          pattern_[--top] = pattern_[--length];
        }
      }
      d_[k] = y_[k];
      y_[k] = 0;
      for (; top < nf; ++top) {
        budget.step(8);
        auto i = pattern_[top];
        double yi = y_[i];
        y_[i] = 0;
        size_t end = lp_[i] + lnz_[i];
        if (end >= lp_[i + 1])
          throw std::logic_error(
              "Free LDL numeric fill exceeded symbolic plan");
        for (size_t at = lp_[i]; at < end; ++at) {
          budget.step(2);
          y_[li_[at]] -= lx_[at] * yi;
        }
        double value = yi / d_[i];
        d_[k] -= value * yi;
        if (!std::isfinite(value) || !std::isfinite(d_[k]))
          throw std::runtime_error("Free LDL factor overflow");
        li_[end] = k;
        lx_[end] = value;
        ++lnz_[i];
      }
      if (!(d_[k] > 0) || !std::isfinite(d_[k]))
        throw std::runtime_error("Free LDL positive pivot invalid");
    }
    for (size_t i = 0; i < nf; ++i) {
      budget.step(1);
      if (lnz_[i] != lp_[i + 1] - lp_[i])
        throw std::logic_error("Free LDL numeric column count differs");
    }
    budget.step(n);
    for (size_t i = 0; i < n; ++i)
      cached_[i] = status[i] != 0;
    if (budget.used - before != predicted_factor_work)
      throw std::logic_error(
          "Free LDL numeric work plan differs from actual count");
    ++factorizations;
    modified_pivots += changed;
    last_free_vertices = nf;
    peak_free_fill = std::max(peak_free_fill, lp_[nf]);
    valid_ = true;
  }

public:
  size_t planned_bytes = 0, matrix_nonzeros = 0, factor_nonzeros = 0,
         predicted_factor_work = 0, factorizations = 0, modified_pivots = 0,
         last_free_vertices = 0, peak_free_fill = 0;
  template <class System, class Budget>
  ScreenedSparseLDL(const System &l, size_t maximum_bytes, Budget &budget) {
    budget.stop();
    const size_t n = l.diagonal.size(), nz = l.columns.size();
    if (!n || n > 2'000'000 || maximum_bytes < 16384 || l.mass.size() != n ||
        l.starts.size() != n + 1 || l.starts.front() != 0 ||
        l.starts.back() != nz || l.values.size() != nz)
      throw std::invalid_argument("Invalid sparse biharmonic matrix domain");
    if (n > (maximum_bytes - 16384) / 512)
      throw std::length_error("Sparse biharmonic metadata exceeds byte cap");
    const size_t pair_cap = (maximum_bytes - n * 512 - 16384) / 384;
    budget.step(n);
    for (size_t i = 0; i < n; ++i)
      if (l.starts[i] > l.starts[i + 1] || l.starts[i + 1] > nz ||
          !(l.mass[i] > 0) || !std::isfinite(l.mass[i]) ||
          !(l.diagonal[i] > 0) || !std::isfinite(l.diagonal[i]))
        throw std::invalid_argument(
            "Invalid sparse Laplacian global row bounds");
    size_t pairs = 0;
    for (size_t i = 0; i < n; ++i) {
      budget.step(1);
      if (l.starts[i] > l.starts[i + 1] || !(l.mass[i] > 0) ||
          !std::isfinite(l.mass[i]) || !(l.diagonal[i] > 0) ||
          !std::isfinite(l.diagonal[i]))
        throw std::invalid_argument("Invalid sparse biharmonic row/mass");
      uint32_t previous = 0;
      bool first = true;
      for (size_t at = l.starts[i]; at < l.starts[i + 1]; ++at) {
        budget.step(1);
        auto j = l.columns[at];
        if (j >= n || j == i || (!first && j <= previous) ||
            !std::isfinite(l.values[at]))
          throw std::invalid_argument(
              "Laplacian rows must be finite sorted unique off-diagonals");
        first = false;
        previous = j;
        size_t left = l.starts[j], right = l.starts[j + 1];
        while (left < right) {
          budget.step(1);
          size_t mid = left + (right - left) / 2;
          if (l.columns[mid] < i)
            left = mid + 1;
          else
            right = mid;
        }
        if (left == l.starts[j + 1] || l.columns[left] != i ||
            l.values[left] != l.values[at])
          throw std::invalid_argument("Laplacian symmetry witness differs");
      }
      // Conservative full-row count bounds the upper triangle even at odd
      // degrees.
      const size_t add = 1 + l.starts[i + 1] - l.starts[i];
      if (add > pair_cap - pairs)
        throw std::length_error("Screened matrix exceeds byte cap");
      pairs += add;
    }
    const size_t construction = n * 512 + pairs * 384 + 16384;
    planned_bytes = construction;
    ap_.assign(n + 1, 0);
    permutation_.reserve(n);
    inverse_.resize(n);
    diagonal_.resize(n);
    {
      using Key = std::array<uint32_t, 2>;
      struct PairCompare {
        Budget *budget;
        bool operator()(const Key &a, const Key &b) const {
          budget->step();
          return a < b;
        }
      };
      struct VertexCompare {
        Budget *budget;
        bool operator()(uint32_t a, uint32_t b) const {
          budget->step();
          return a < b;
        }
      };
      std::map<Key, double, PairCompare> matrix(PairCompare{&budget});
      for (uint32_t i = 0; i < n; ++i) {
        budget.step(1);
        matrix[{i, i}] = l.diagonal[i];
        for (size_t at = l.starts[i]; at < l.starts[i + 1]; ++at) {
          budget.step(1);
          if (l.columns[at] > i)
            matrix[{i, l.columns[at]}] = l.values[at];
        }
      }
      for (auto at = matrix.begin(); at != matrix.end();) {
        budget.step(1);
        if (at->first[0] == at->first[1] && !(at->second > 0))
          throw std::invalid_argument(
              "Sparse biharmonic diagonal is not representable");
        if (at->first[0] != at->first[1] && at->second == 0)
          at = matrix.erase(at);
        else
          ++at;
      }
      matrix_nonzeros = matrix.size();
      std::vector<std::set<uint32_t, VertexCompare>> adjacency;
      adjacency.reserve(n);
      for (size_t i = 0; i < n; ++i)
        adjacency.emplace_back(VertexCompare{&budget});
      for (const auto &[key, value] : matrix) {
        (void)value;
        budget.step(1);
        if (key[0] != key[1]) {
          adjacency[key[0]].insert(key[1]);
          adjacency[key[1]].insert(key[0]);
        }
      }
      std::vector<uint32_t> heap(n), position(n), neighbors;
      neighbors.reserve(n);
      std::iota(heap.begin(), heap.end(), 0);
      std::iota(position.begin(), position.end(), 0);
      auto less = [&](uint32_t a, uint32_t b) {
        budget.step(1);
        return adjacency[a].size() != adjacency[b].size()
                   ? adjacency[a].size() < adjacency[b].size()
                   : a < b;
      };
      auto exchange = [&](size_t a, size_t b) {
        std::swap(heap[a], heap[b]);
        position[heap[a]] = uint32_t(a);
        position[heap[b]] = uint32_t(b);
      };
      auto up = [&](size_t at) {
        while (at) {
          size_t p = (at - 1) / 2;
          if (!less(heap[at], heap[p]))
            break;
          exchange(at, p);
          at = p;
        }
      };
      auto down = [&](size_t at) {
        while (at < heap.size()) {
          size_t child = at * 2 + 1;
          if (child >= heap.size())
            break;
          if (child + 1 < heap.size() && less(heap[child + 1], heap[child]))
            ++child;
          if (!less(heap[child], heap[at]))
            break;
          exchange(child, at);
          at = child;
        }
      };
      for (size_t i = n / 2; i-- > 0;)
        down(i);
      size_t fill_bytes = 0;
      while (!heap.empty()) {
        budget.stop();
        auto vertex = heap.front();
        exchange(0, heap.size() - 1);
        heap.pop_back();
        position[vertex] = UINT32_MAX;
        if (!heap.empty())
          down(0);
        permutation_.push_back(vertex);
        neighbors.assign(adjacency[vertex].begin(), adjacency[vertex].end());
        for (size_t a = 0; a < neighbors.size(); ++a)
          for (size_t b = a + 1; b < neighbors.size(); ++b) {
            budget.step(1);
            auto i = neighbors[a], j = neighbors[b];
            if (adjacency[i].find(j) == adjacency[i].end()) {
              if (192 > maximum_bytes - construction - fill_bytes)
                throw std::length_error(
                    "Minimum-degree fill exceeds byte cap before insertion");
              fill_bytes += 192;
              adjacency[i].insert(j);
              up(position[i]);
              down(position[i]);
              adjacency[j].insert(i);
              up(position[j]);
              down(position[j]);
            }
          }
        for (auto i : neighbors) {
          budget.step(1);
          adjacency[i].erase(vertex);
          up(position[i]);
          down(position[i]);
        }
        adjacency[vertex].clear();
      }
      planned_bytes = construction + fill_bytes;
      for (uint32_t k = 0; k < n; ++k)
        inverse_[permutation_[k]] = k;
      for (const auto &[key, value] : matrix) {
        (void)value;
        budget.step(1);
        auto i = inverse_[key[0]], j = inverse_[key[1]];
        ++ap_[std::max(i, j) + 1];
      }
      for (size_t i = 0; i < n; ++i)
        ap_[i + 1] += ap_[i];
      ai_.resize(matrix.size());
      ax_.resize(matrix.size());
      auto cursor = ap_;
      for (const auto &[key, value] : matrix) {
        budget.step(1);
        auto i = inverse_[key[0]], j = inverse_[key[1]];
        if (i > j)
          std::swap(i, j);
        auto at = cursor[j]++;
        ai_[at] = i;
        ax_[at] = value;
        if (i == j)
          diagonal_[i] = value;
      }
    }
    parent_.assign(n, -1);
    flag_.assign(n, -1);
    lnz_.assign(n, 0);
    lp_.assign(n + 1, 0);
    for (uint32_t k = 0; k < n; ++k) {
      budget.stop();
      parent_[k] = -1;
      flag_[k] = int32_t(k);
      lnz_[k] = 0;
      for (size_t at = ap_[k]; at < ap_[k + 1]; ++at) {
        budget.step(1);
        auto i = ai_[at];
        if (i < k)
          while (flag_[i] != int32_t(k)) {
            budget.step(4);
            if (parent_[i] < 0)
              parent_[i] = int32_t(k);
            ++lnz_[i];
            flag_[i] = int32_t(k);
            i = uint32_t(parent_[i]);
          }
      }
    }
    predicted_factor_work = n * 11 + matrix_nonzeros * 4;
    for (size_t i = 0; i < n; ++i) {
      budget.step(1);
      lp_[i + 1] = lp_[i] + lnz_[i];
      predicted_factor_work += size_t(lnz_[i]) * (lnz_[i] + 12);
    }
    factor_nonzeros = lp_.back();
    const size_t factor_bytes =
        n * 192 + matrix_nonzeros * 32 + factor_nonzeros * 16 + 32768;
    if (factor_bytes > maximum_bytes)
      throw std::length_error(
          "Sparse LDL symbolic fill exceeds byte cap before allocation");

    planned_bytes = std::max(planned_bytes, factor_bytes);
    li_.resize(factor_nonzeros);
    lx_.resize(factor_nonzeros);
    d_.resize(n);
    y_.resize(n);
    scratch_.resize(n);
    pattern_.resize(n);
    cached_.resize(n);
    free_ap_.resize(n + 1);
    free_ai_.resize(matrix_nonzeros);
    free_ax_.resize(matrix_nonzeros);
    free_index_.resize(n);
    free_old_.resize(n);
  }
  template <class Budget>
  void apply(std::span<const double> r, std::span<double> z,
             std::span<const uint8_t> status, Budget &budget) {
    const size_t n = d_.size();
    if (r.size() != n || z.size() != n || status.size() != n)
      throw std::invalid_argument("Sparse LDL free-mask dimensions differ");
    bool same = valid_;
    budget.step(n);
    for (size_t i = 0; i < n; ++i) {
      if (status[i] > 3)
        throw std::invalid_argument("Invalid sparse LDL working constraint");
      same &= cached_[i] == (status[i] != 0);
    }
    if (!same)
      factor(status, budget);
    const size_t nf = free_n_;
    for (size_t k = 0; k < nf; ++k) {
      budget.step(1);
      scratch_[k] = r[permutation_[free_old_[k]]];
    }
    for (size_t j = 0; j < nf; ++j) {
      budget.stop();
      for (size_t at = lp_[j]; at < lp_[j + 1]; ++at) {
        budget.step(1);
        scratch_[li_[at]] -= lx_[at] * scratch_[j];
      }
    }
    for (size_t j = 0; j < nf; ++j) {
      budget.step(1);
      scratch_[j] /= d_[j];
    }
    for (size_t j = nf; j-- > 0;) {
      budget.stop();
      for (size_t at = lp_[j]; at < lp_[j + 1]; ++at) {
        budget.step(1);
        scratch_[j] -= lx_[at] * scratch_[li_[at]];
      }
    }
    budget.step(n);
    std::fill(z.begin(), z.end(), 0);
    for (size_t k = 0; k < nf; ++k) {
      budget.step(1);
      if (!std::isfinite(scratch_[k]))
        throw std::runtime_error("Free sparse LDL produced nonfinite output");
      z[permutation_[free_old_[k]]] = scratch_[k];
    }
  }
};
} // namespace dome::detail
