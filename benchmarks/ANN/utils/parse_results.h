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

#include <algorithm>
#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "common/geometry.h"
#include "indexTools.h"
#include <set>

struct Graph{
  std::string name;
  std::string params;
  long size;
  double avg_deg;
  int max_deg;
  double time;

  Graph(std::string n, std::string p, long s, double ad, int md, double t) : name(n), 
    params(p), size(s), avg_deg(ad), max_deg(md), time(t) {}
  
  void print(){
    std::cout << name << " graph built with " << size << " points and parameters " << params << std::endl;
    std::cout << "Graph has average degree " << avg_deg << " and maximum degree " << max_deg << std::endl;
    std::cout << "Graph built in " << time << " seconds" << std::endl;
  }
};

struct range_result{
  int num_queries;
  int num_nonzero_queries;

  double recall;
  double alt_recall;

  int avg_cmps;
  int tail_cmps;

  int avg_visited;
  int tail_visited;

  float QPS;

  int k;
  int beamQ;
  float cut;
  double slack;

  range_result(int nq, int nnq, double r, double r2, parlay::sequence<int> stats, 
  float qps, int K, int Q, float c, float s) : 
    num_queries(nq), num_nonzero_queries(nnq), recall(r), alt_recall(r2), QPS(qps), k(K), beamQ(Q), cut(c), slack(s) {

    if(stats.size() != 4) abort();

    avg_cmps = stats[0]; tail_cmps = stats[1];
    avg_visited = stats[2]; tail_visited = stats[3];
  }

  void print(){
    std::cout << "k = " << k << ", Q = " << beamQ << ", cut = " << cut << ", slack = " << slack
	    << ", throughput = " << QPS << "/second" << std::endl;
    std::cout << std::endl;
    std::cout << "Num nonzero queries: " << num_nonzero_queries << std::endl;
    std::cout << "Nonzero recall: " << recall << std::endl; 
    std::cout << "Alternate recall: " << alt_recall;
    std::cout << std::endl;
  	std::cout << "Average dist cmps: " << avg_cmps << ", 99th percentile dist cmps: " << tail_cmps << std::endl;
  	std::cout << "Average num visited: " << avg_visited << ", 99th percentile num visited: " << tail_visited << std::endl;
  }
};

struct nn_result{
  double recall;

  int avg_cmps;
  int tail_cmps;

  int avg_visited;
  int tail_visited;

  float QPS;

  int k;
  int beamQ;
  float cut;

  long num_queries;

  nn_result(double r, parlay::sequence<int> stats, float qps, int K, int Q, float c, long q) : recall(r), 
    QPS(qps), k(K), beamQ(Q), cut(c), num_queries(q) {

    if(stats.size() != 4) abort();

    avg_cmps = stats[0]; tail_cmps = stats[1];
    avg_visited = stats[2]; tail_visited = stats[3];
  }

  void print(){
    std::cout << "Over " << num_queries << " queries" << std::endl;
    std::cout << "k = " << k << ", Q = " << beamQ << ", cut = " << cut 
	    << ", throughput = " << QPS << "/second" << std::endl;
    std::cout << "Recall: " << recall << std::endl; 
  	std::cout << "Average dist cmps: " << avg_cmps << ", 99th percentile dist cmps: " << tail_cmps << std::endl;
  	std::cout << "Average num visited: " << avg_visited << ", 99th percentile num visited: " << tail_visited << std::endl;
  }
};

template<typename res>
auto parse_result(parlay::sequence<res> results, parlay::sequence<float> buckets){
  parlay::sequence<float> ret_buckets;
  parlay::sequence<res> retval;
  for(int i=0; i<buckets.size(); i++){
    float b = buckets[i];
    auto pred = [&] (res R) {return R.recall >= b;};
    parlay::sequence<res> candidates;
    auto temp_candidates = parlay::filter(results, pred);
    if((i == buckets.size()-1) || (temp_candidates.size() == 0)){
      candidates = temp_candidates;
    }else{
      float c = buckets[i+1];
      auto pred2 = [&] (res R) {return R.recall <= c;};
      candidates = parlay::filter(temp_candidates, pred2);
    }
    if(candidates.size() != 0){
      auto less = [&] (res R, res S) {return R.QPS < S.QPS;};
      res M = *(parlay::max_element(candidates, less));
      std::cout << "For recall above: " << b << std::endl;
      M.print();
      retval.push_back(M);
      ret_buckets.push_back(b);
      std::cout << std::endl;
      std::cout << std::endl;
    }
  }
  return std::make_pair(retval, ret_buckets);
}