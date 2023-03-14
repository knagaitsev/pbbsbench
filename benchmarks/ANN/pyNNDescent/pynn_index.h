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
#include "parlay/random.h"
#include "common/geometry.h"
#include <random>
#include <set>
#include <queue>
#include <math.h>
#include "../utils/clusterPynn.h"


extern bool report_stats;
template<typename T>
struct pyNN_index{
	int K;
	unsigned d;
	float delta;
    bool mips; 
	using tvec_point = Tvec_point<T>;
	using edge = std::pair<int, int>;
    using pid = std::pair<int,float>;
    using labelled_edge = std::pair<int, pid>;

    float Distance(T* p, T* q, unsigned d){
		if(mips) return mips_distance(p, q, d);
		else return distance(p, q, d);
	}

    static constexpr auto less = [] (edge a, edge b) {return a.second < b.second;};

    pyNN_index(int md, unsigned dim, float Delta, bool m) : K(md), d(dim), delta(Delta), mips(m) {}

    parlay::sequence<parlay::sequence<pid>> old_neighbors;

    void push_into_queue(std::priority_queue<edge, std::vector<edge>, decltype(less)> &Q, edge p){
        if(Q.size() < 2*K){
            Q.push(p);
        } else{
            int highest_p = Q.top().second;
            if(p.second < highest_p){
                Q.pop();
                Q.push(p);
            }
        }
    }

    parlay::sequence<int> nn_descent(parlay::sequence<tvec_point*> &v, parlay::sequence<int> &changed){
        auto new_changed = parlay::sequence<int>(v.size(), 0);
        auto rev = reverse_graph();
        std::cout << "generated reverse graph" << std::endl;
        parlay::random_generator gen;
        size_t n=v.size();
        std::uniform_int_distribution<int> dis(0, n-1);
        int batch_size = 1000000;
        std::pair<int, parlay::sequence<int>> *begin;
		std::pair<int, parlay::sequence<int>> *end = rev.begin();
		int counter = 0;
		while(end != rev.end()){
			counter++;
			begin = end;
			int remaining = rev.end() - end;
			end += std::min(remaining, batch_size);
			nn_descent_chunk(v, changed, new_changed, begin, end);
		}
        return new_changed;
    }

    void nn_descent_chunk(parlay::sequence<tvec_point*> &v, parlay::sequence<int> &changed, 
		parlay::sequence<int> &new_changed, std::pair<int, parlay::sequence<int>> *begin, 
		std::pair<int, parlay::sequence<int>> *end){
        size_t stride = end - begin;
		auto less = [&] (pid a, pid b) {return a.second < b.second;};
        auto grouped_labelled = parlay::tabulate(stride, [&] (size_t i){
            int index = (begin+i)->first;
            std::set<int> to_filter;
            to_filter.insert(index);
            for(int j=0; j<old_neighbors[index].size(); j++){
                to_filter.insert(old_neighbors[index][j].first);
            }
            auto f = [&] (int a) {return (to_filter.find(a) == to_filter.end());};
            auto filtered_candidates = parlay::filter((begin+i)->second, f);
			parlay::sequence<labelled_edge> edges = parlay::sequence<labelled_edge>();
			for(int l=0; l<filtered_candidates.size(); l++){
                int j=filtered_candidates[l];
				for(int m=l+1; m<filtered_candidates.size(); m++){
                    int k=filtered_candidates[m];
                    float dist = Distance(v[j]->coordinates.begin(), v[k]->coordinates.begin(), d);
                    float j_max = old_neighbors[j][old_neighbors[j].size()-1].second;
                    float k_max = old_neighbors[k][old_neighbors[k].size()-1].second;
                    if(dist < j_max) edges.push_back(std::make_pair(j, std::make_pair(k, dist)));
                    if(dist < k_max) edges.push_back(std::make_pair(k, std::make_pair(j, dist)));
				}
			}
            for(int l=0; l<old_neighbors[index].size(); l++){
                int j = old_neighbors[index][l].first;
                for(const int& k : filtered_candidates){
                    float dist = Distance(v[j]->coordinates.begin(), v[k]->coordinates.begin(), d);
                    float j_max = old_neighbors[j][old_neighbors[j].size()-1].second;
                    float k_max = old_neighbors[k][old_neighbors[k].size()-1].second;
                    if(dist < j_max) edges.push_back(std::make_pair(j, std::make_pair(k, dist)));
                    if(dist < k_max) edges.push_back(std::make_pair(k, std::make_pair(j, dist)));                   
                }
            }
			return edges;
		});
        auto candidates = parlay::group_by_key(parlay::flatten(grouped_labelled));
        parlay::parallel_for(0, candidates.size(), [&] (size_t i){
            auto less2 = [&] (pid a, pid b) {
                if(a.second < b.second) return true;
                else if(a.second == b.second){
                    if(a.first < b.first) return true;
                }
                return false;
            };
            parlay::sort_inplace(candidates[i].second, less2);
            int cur_index=-1;
            parlay::sequence<pid> filtered_candidates;
            for(const pid& p : candidates[i].second){
                if(p.first!=cur_index){
                    filtered_candidates.push_back(p);
                    cur_index = p.first;
                }
            }
            int index = candidates[i].first;
            auto [new_edges, change] = seq_union_bounded(old_neighbors[index], filtered_candidates, K);
            if(change){
                new_changed[index]=1;
                old_neighbors[index]=new_edges;
            }
        });
    }

    parlay::sequence<std::pair<int, parlay::sequence<int>>> reverse_graph(){
        parlay::sequence<parlay::sequence<edge>> to_group = parlay::tabulate(old_neighbors.size(), [&] (size_t i){
            size_t s = old_neighbors[i].size();
            parlay::sequence<edge> e(s);
            for(int j=0; j<s; j++){
                e[j] = std::make_pair(old_neighbors[i][j].first, (int) i);
            }
            return e; 
        });
        auto sorted_graph =  parlay::group_by_key(parlay::flatten(to_group));
        parlay::parallel_for(0, sorted_graph.size(), [&] (size_t i){
            auto shuffled = parlay::remove_duplicates(parlay::random_shuffle(sorted_graph[i].second, i));
            int upper_bound = std::min((int) shuffled.size(), K);
            auto truncated = parlay::tabulate(upper_bound, [&] (size_t j){
                return shuffled[j];
            });
            sorted_graph[i].second = truncated;
        });
        return sorted_graph;
    }

    int nn_descent_wrapper(parlay::sequence<tvec_point*> &v){
		size_t n = v.size();
		auto changed = parlay::tabulate(n, [&] (size_t i) {return 1;});
		int rounds = 0;
        int max_rounds = std::max(10, (int) log2(d));
		while(parlay::reduce(changed) >= delta*n && rounds < 10){
			std::cout << "Round " << rounds << std::endl; 
			auto new_changed = nn_descent(v, changed);
			changed = new_changed;
			rounds++;
			std::cout << parlay::reduce(changed) << " elements changed" << std::endl; 
		}
		std::cout << "descent converged in " << rounds << " rounds" << std::endl; 
		return rounds;
	}

    void undirect_and_prune(parlay::sequence<tvec_point*> &v, float alpha){
        parlay::sequence<parlay::sequence<edge>> to_group = parlay::tabulate(old_neighbors.size(), [&] (size_t i){
            size_t s = old_neighbors[i].size();
            parlay::sequence<edge> e(s);
            for(int j=0; j<s; j++){
                e[j] = std::make_pair(old_neighbors[i][j].first, (int) i);
            }
            return e; 
        });
        auto undirected_graph = parlay::group_by_key_ordered(parlay::flatten(to_group));
        parlay::parallel_for(0, undirected_graph.size(), [&] (size_t i){
            int index = undirected_graph[i].first;
            auto filtered = parlay::remove_duplicates(undirected_graph[i].second);
            auto undirected_pids = parlay::tabulate(filtered.size(), [&] (size_t j){
                int indexU = filtered[j];
                float dist = Distance(v[index]->coordinates.begin(), v[indexU]->coordinates.begin(), d);
                return std::make_pair(indexU, dist);
            });
            parlay::sort_inplace(undirected_pids, less);
            auto merged_pids = seq_union(old_neighbors[index], undirected_pids);
            old_neighbors[index] = merged_pids;
        });
        parlay::parallel_for(0, v.size(), [&] (size_t i){
            parlay::sequence<int> new_out = parlay::sequence<int>();
			for(const pid& j : old_neighbors[i]){
				if(new_out.size() == K) break;
				else if(new_out.size() == 0) new_out.push_back(j.first);
				else{
					float dist_p = j.second;
					bool add = true;
					for(const int& k : new_out){
						float dist = Distance(v[j.first]->coordinates.begin(), v[k]->coordinates.begin(), d);
						if(dist_p > alpha*dist) {add = false; break;}
					}
					if(add) new_out.push_back(j.first);
				}
			}
			add_out_nbh(new_out, v[i]);
        });
    }

    void assign_edges(parlay::sequence<tvec_point*> &v){
		parlay::parallel_for(0, v.size(), [&] (size_t i){
			for(size_t j=0; j<old_neighbors[i].size(); j++){
				int out_nbh = old_neighbors[i][j].first;
				add_nbh(out_nbh, v[i]);
			}
		});
	}

    void build_index(parlay::sequence<tvec_point*> &v, size_t cluster_size, int num_clusters, double alpha){
        std::cout << "Mips: " << mips << std::endl;
		clear(v);
		clusterPID<T> C(d, mips);
        old_neighbors = parlay::sequence<parlay::sequence<pid>>(v.size());
		C.multiple_clustertrees(v, cluster_size, num_clusters, d, K, old_neighbors);
		nn_descent_wrapper(v);
		undirect_and_prune(v, alpha);
	}
};