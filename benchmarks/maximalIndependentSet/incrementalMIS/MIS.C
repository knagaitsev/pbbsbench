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

#include <iostream>
#include "parlay/primitives.h"
#include "parlay/random.h"
#include "common/graph.h"
#include "MIS.h"

template<typename T>
  parlay::sequence<T> rand(size_t s, size_t e) {
    parlay::random_generator gen(0);
    std::uniform_real_distribution<double> dis(0.0,1.0);
    auto In = parlay::tabulate(e-s, [&] (size_t i) -> T{
      auto r = gen[i+s];
      return dis(r);});
    return In;
  }

// **************************************************************
//    MAXIMAL INDEPENDENT SET
// **************************************************************

parlay::sequence<char> maximalIndependentSet(Graph const &G)
{
  size_t n = G.n;
  parlay::sequence<char> Flags(n, (char) 0);

  // auto G = graph<uint, uint>(origGraph.offsets, origGraph.edges, n);

  // parlay::sequence<uint> rands(n, 0);

  parlay::sequence<bool> toRemove(n, false);
  parlay::sequence<bool> removed(n, false);
  parlay::sequence<int> activeFlags(n, 0);
  parlay::sequence<double> ranks(n, 0.0);

  parlay::random_generator gen(0);
  std::uniform_real_distribution<double> dis(0.0, 1.0);

  // int iter = 0;

  while (true) {
    // std::cout << "Iter: " << iter << "\n";
    // iter++;

    // for (size_t i = 0; i < n; i++) {
    //   rands[i] = dis(gen);
    // }

    bool allRemoved = true;
    parlay::parallel_for(0, n, [&] (size_t i) {
    // for (size_t i = 0; i < n; i++) {
      removed[i] = toRemove[i];
      if (!toRemove[i]) {
        allRemoved = false;

        // auto rand = rands[i];
        auto rand = dis(gen);

        uint deg = 0;
        for (size_t j = 0; j < G[i].degree; j++) {
          vertexId ngh = G[i].Neighbors[j];
          deg += (uint)(!toRemove[ngh]);
        }

        int activeFlag = deg * rand < 0.5;
        // int activeFlag = rand < 0.5;
        activeFlags[i] = activeFlag;

        double rank = (double)activeFlag * ((double)(deg) + (1.0)/((double)(i + 1)));
        ranks[i] = rank;
      }
    });

    if (allRemoved) {
      break;
    }

    parlay::parallel_for(0, n, [&] (size_t i) {
    // for (size_t i = 0; i < n; i++) {
      if (!removed[i] && activeFlags[i]) {
        double myRank = ranks[i];

        bool good = true;
        for (size_t j = 0; j < G[i].degree; j++) {
          vertexId ngh = G[i].Neighbors[j];
          if (!removed[ngh] && myRank <= ranks[ngh]) {
            good = false;
            break;
          }
        }

        if (good) {
          Flags[i] = 1;
          // removed[i] = true;
          toRemove[i] = true;
          for (size_t j = 0; j < G[i].degree; j++) {
            vertexId ngh = G[i].Neighbors[j];
            Flags[ngh] = 2;
            // removed[ngh] = true;
            toRemove[ngh] = true;
          }
        }
      }
    });

    // parlay::parallel_for(0, n, [&] (size_t i) {
    // // for (size_t i = 0; i < n; i++) {
    //   removed[i] = toRemove[i];
    // });
  }
  // for (size_t i = 0; i < n; i++) {
  //   Flags[i] = 1;
  //   for (size_t j = 0; j< G[i].degree; j++) {
  //     vertexId ngh = G[i].Neighbors[j];
  //     if (Flags[ngh] == 1) {
	// Flags[i] = 2;
	// break;
  //     }
  //   }
  // }

  // int c = 0;
  // for (size_t i = 0; i < n; i++) {
  //   if (Flags[i] == 1) {
  //     c++;
  //   }
  // }
  // std::cout << "Flags len: " << c << "\n";

  return Flags;
}
