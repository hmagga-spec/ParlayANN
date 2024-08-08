/* structs which should take a PointRange argument and return a sequence of
 * sequences representing clusters */
#ifndef CLUSTERING_H
#define CLUSTERING_H

#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/sequence.h"

#include "../../algorithms/HCNNG/clusterEdge.h"
#include "utils/graph.h"
#include "utils/point_range.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

template <typename Point, typename PointRange, typename indexType>
using cluster_struct = cluster<Point, PointRange, indexType>;

extern size_t max_iter = 20;



template <class Point, class PointRange, typename indexType>
struct HCNNGClusterer {
  size_t cluster_size = 1000;
  Graph<indexType>
     G;   // not actually used but needed as a function argument for clusterEdge

  HCNNGClusterer() {}

  HCNNGClusterer(size_t cluster_size) : cluster_size(cluster_size) {}

  /* This function should return sequences of sequences describing a partition
   * of `indices`, and should not subsequently require mapping the indices used
   * in the output to their actual values */
  parlay::sequence<parlay::sequence<indexType>> cluster(
     PointRange points, parlay::sequence<indexType> indices) {
    size_t num_points = indices.size();
    size_t n = points.size();
    // we make a very sparse sequence to store the clusters
    parlay::sequence<std::pair<indexType*, indexType>> clusters =
       parlay::sequence<std::pair<indexType*, indexType>>(n);
    // lambda to assign a cluster
    auto assign = [&](Graph<indexType> G, PointRange points,
                      parlay::sequence<indexType>& active_indices,
                      long MSTDeg) {
      if (active_indices.size() == 0) {   // this should never happen
        // std::cout << "HCNNGClusterer::cluster: active_indices.size() == 0" <<
        // std::endl;
        throw std::runtime_error(
           "HCNNGClusterer::cluster: active_indices.size() == 0");
        return;
      }
      if (std::max_element(active_indices.begin(), active_indices.end())[0] >=
          n) {
        // std::cout << "lambda receiving oversized index" << std::endl;
        throw std::runtime_error("lambda receiving oversized index");
        return;
      }
      indexType cluster_index = active_indices[0];

      indexType* cluster = new indexType[active_indices.size()];

      std::memcpy(cluster, active_indices.begin(),
                  active_indices.size() * sizeof(indexType));


      auto tmp = std::make_pair(cluster, active_indices.size());
      clusters[cluster_index] = tmp;
      return;
    };


    //should populate the clusters sequence
    cluster_struct<Point, PointRange, indexType>().active_indices_rcw(
       G, points, indices, cluster_size, assign, 0);



    // remove empty clusters
    // clusters = parlay::filter(clusters, [&] (parlay::sequence<indexType>
    // cluster) {return cluster.size() > 0;});
    clusters = parlay::filter(clusters,
                              [&](std::pair<indexType*, indexType> cluster) {
                                return cluster.second > 0;
                              });
    parlay::sequence<parlay::sequence<indexType>> result(clusters.size());

    for (size_t i = 0; i < clusters.size(); i++) {
      result[i] = parlay::sequence<indexType>(
         clusters[i].first, clusters[i].first + clusters[i].second);
    }

    // free the memory allocated for the clusters
    for (size_t i = 0; i < clusters.size(); i++) {
      delete[] clusters[i].first;
    }

    return result;
  }

  parlay::sequence<parlay::sequence<indexType>> cluster(PointRange points) {
    auto active_indices =
       parlay::tabulate(points.size(), [&](indexType i) { return i; });
    return this->cluster(points, active_indices);
  }

  std::pair<size_t, size_t> get_build_params() const { return std::make_pair(cluster_size, 0); }
};

template <typename T, class Point, typename indexType>
struct KMeansClusterer {
  size_t n_toplevel_clusters = 1000;
  size_t n_subclusters = 1000;

  size_t max_iters; 
  size_t toplevel_subsample = 500; // Subsample rate.
  size_t subsample = 5;

  KMeansClusterer() {}

  KMeansClusterer(size_t nt, size_t nc) : n_toplevel_clusters(nt), n_subclusters(nc) {
    this->max_iters = max_iter;
  }

  parlay::sequence<parlay::sequence<size_t>> get_clusters(parlay::sequence<size_t>& cluster_assignments, size_t num_clusters) {
    auto pairs = parlay::tabulate(cluster_assignments.size(), [&] (size_t i) {
      return std::make_pair(cluster_assignments[i], i);
    });
    return parlay::group_by_index(pairs, num_clusters);
  }

	void cluster_stats(parlay::sequence<parlay::sequence<indexType>>& clusters) {
		auto sizes = parlay::delayed_seq<size_t>(clusters.size(), [&] (size_t i) {
			return clusters[i].size();
		});
		size_t num_points = parlay::reduce(sizes);
		size_t num_clusters = clusters.size();
		size_t min_size = parlay::reduce(sizes, parlay::minm<size_t>());
		size_t max_size = parlay::reduce(sizes, parlay::maxm<size_t>());
		size_t avg_size = num_points / num_clusters;
		std::cout << "ClusterStats: num_points: " << num_points << " num_clusters: " << num_clusters << " Min: " << min_size << " Max: " << max_size << " Avg: " << avg_size << std::endl;
	}

 

  parlay::sequence<parlay::sequence<indexType>> cluster_impl(PointRange<T, Point> &points, parlay::sequence<indexType> &input_indices, size_t num_clusters, size_t subsample_rate, size_t rng=0){
    parlay::internal::timer  t;
    t.start();
    
    // generate subsample
    // need to use a seed here so that the two-level clusters don't all generate the same subsample
    auto permuted_indices = parlay::random_shuffle(input_indices, rng);
    auto indices = parlay::tabulate(input_indices.size() / subsample_rate, [&] (size_t i) {
      return permuted_indices[i];
    });

    // initialize centroids
    size_t num_points = indices.size();
    size_t dim = points.dimension();
    size_t aligned_dim = points.aligned_dimension();
    std::cout << "KMeans run on: " << num_points << " many points to obtain: " << num_clusters << " many clusters." << std::endl;
    auto centroid_data = parlay::sequence<T>(num_clusters * aligned_dim);
    auto centroids = parlay::tabulate(num_clusters, [&](size_t i) {
      return Point(centroid_data.begin() + i * aligned_dim, dim, aligned_dim, i);
    });

    // initially run HCNNGClusterer to get initial set of clusters
    // clusters is type sequence<sequence<indexType>>
    auto clusters = HCNNGClusterer<Point, PointRange<T, Point>, indexType>(
                       num_points / num_clusters)
                       .cluster(points, input_indices);

    //sort by size -- why?
    parlay::sort_inplace(clusters, [&](parlay::sequence<indexType> a,
                               parlay::sequence<indexType> b) {
      return a.size() < b.size();
    });

        //compute centroid of each cluster
    parlay::parallel_for(0, num_clusters, [&](size_t i) {
      size_t offset = i * aligned_dim;
      parlay::sequence<double> centroid(dim);
      for (size_t j = 0; j < clusters[i].size(); j++) {
        for (size_t d = 0; d < dim; d++) {
          centroid[d] += static_cast<double>(points[clusters[i][j]][d]) /
                         clusters[i].size();
        }
      }
      for (size_t d = 0; d < dim; d++) {
        centroid_data[offset + d] = static_cast<T>(std::round(centroid[d]));
      }
    });

    //assign each point in subsample to nearest centroid
    parlay::sequence<size_t> cluster_assignments =
       parlay::sequence<size_t>::uninitialized(num_points);

    parlay::parallel_for(0, num_points, [&](size_t i) {
      double min_dist = std::numeric_limits<double>::max();
      size_t min_index = 0;
      for (size_t j = 0; j < num_clusters; j++) {
        double dist = points[indices[i]].distance(centroids[j]);
        if (dist < min_dist) {
          min_dist = dist;
          min_index = j;
        }
      }
      cluster_assignments[i] = min_index;
    });

    // now run k-means
    bool not_converged;
    size_t num_iters = 0;
    do {
      std::cout << "Beginning iteration " << num_iters << "..." << std::endl;
      num_iters++;
      not_converged = false;

      auto new_clusters = get_clusters(cluster_assignments, num_clusters);

      // compute new centroids
      parlay::parallel_for(0, new_clusters.size(), [&](size_t i) {
        size_t offset = i * aligned_dim;
        parlay::sequence<double> centroid(dim);
        auto clust_size = new_clusters[i].size();
        for (size_t j = 0; j < clust_size; j++) {
          auto pt = points[indices[new_clusters[i][j]]];
          for (size_t d = 0; d < dim; d++) {
            centroid[d] +=
               static_cast<double>(pt[d]) / clust_size;
          }
        }
        for (size_t d = 0; d < dim; d++) {
          centroid_data[offset + d] = static_cast<T>(std::round(centroid[d]));
        }
      });

      // compute new assignments
      parlay::parallel_for(0, num_points, [&](size_t i) {
        double min_dist =
           points[indices[i]].distance(centroids[cluster_assignments[i]]);
        size_t min_index = cluster_assignments[i];
        for (size_t j = 0; j < num_clusters; j++) {
          double dist = points[indices[i]].distance(centroids[j]);
          if (dist < min_dist) {
            min_dist = dist;
            min_index = j;
            not_converged = true;
          }
        }
        cluster_assignments[i] = min_index;
      });
    } while (not_converged && num_iters < max_iters);

    num_points = input_indices.size();
    parlay::sequence<size_t> all_cluster_assignments =
       parlay::sequence<size_t>::uninitialized(num_points);

    parlay::parallel_for(0, num_points, [&](size_t i) {
      double min_dist = std::numeric_limits<double>::max();
      size_t min_index = 0;
      for (size_t j = 0; j < num_clusters; j++) {
        double dist = points[input_indices[i]].distance(centroids[j]);
        if (dist < min_dist) {
          min_dist = dist;
          min_index = j;
        }
      }
      all_cluster_assignments[i] = min_index;
    });

    auto output = parlay::tabulate(num_points, [&](size_t i) {
      return std::make_pair(all_cluster_assignments[i], input_indices[i]);
    });


    std::cout << "KMeansClustering Time: " << t.stop() << std::endl;
    auto ret_clusters = parlay::group_by_index(output, num_clusters);
		cluster_stats(ret_clusters);
		return ret_clusters;
  }

   parlay::sequence<parlay::sequence<indexType>> cluster(
     PointRange<T, Point> points, parlay::sequence<indexType> input_indices) {
   
      return cluster_impl(points, input_indices, n_toplevel_clusters, toplevel_subsample);
   
  }


  // Two round cluster in clusterer
  parlay::sequence<parlay::sequence<indexType>> two_round_cluster(
     PointRange<T, Point> &points, parlay::sequence<indexType> &input_indices) {
    parlay::internal::timer  t;
    t.start();

    //compute top-level cluster
    parlay::sequence<parlay::sequence<indexType>> top_level_cluster = cluster_impl(points, input_indices, n_toplevel_clusters, toplevel_subsample);

    parlay::sequence<parlay::sequence<parlay::sequence<indexType>>> two_level_clustering(top_level_cluster.size());
    //compute low-level clusters
    for(size_t i=0; i<top_level_cluster.size(); i++){
      two_level_clustering[i] = cluster_impl(points, top_level_cluster[i], n_subclusters, subsample, i+1);
    }

    parlay::sequence<parlay::sequence<indexType>> final_clustering = parlay::flatten(two_level_clustering);

    std::cout << "Total Two-Level Clustering Time: " << t.stop() << std::endl;
    cluster_stats(final_clustering);
		return final_clustering;


  }

  std::pair<size_t, size_t> get_build_params() const { return std::make_pair(n_toplevel_clusters*n_subclusters, subsample); }
};

#endif   // CLUSTERING_H