#pragma once

#include "types.hpp"

#include <cstddef>
#include <iostream>

/**
 * Extract the region graph from a segmentation. Edges are annotated with the 
 * maximum affinity between the regions.
 *
 * @param aff [in]
 *              The affinity graph to read the affinities from.
 * @param seg [in]
 *              The segmentation.
 * @param max_segid [in]
 *              The highest ID in the segmentation.
 * @param statisticsProvider [in]
 *              A statistics provider to update on-the-fly.
 * @param region_graph [out]
 *              A reference to a region graph to store the result.
 */
template<typename AG, typename V, typename StatisticsProviderType>
inline
void
get_region_graph(
		const AG& aff,
		const V& seg,
		std::size_t max_segid,
		StatisticsProviderType& statisticsProvider,
		RegionGraph<typename V::element>& rg) {

	typedef typename AG::element F;
	typedef typename V::element ID;
	typedef RegionGraph<ID> RegionGraphType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;

	std::ptrdiff_t zdim = aff.shape()[1];
	std::ptrdiff_t ydim = aff.shape()[2];
	std::ptrdiff_t xdim = aff.shape()[3];

	// Use raw pointers for direct memory access instead of boost operator[][]
	const ID* seg_data = seg.data();
	const F* aff_data = aff.data();
	const std::size_t slice_size = ydim * xdim;
	const std::size_t aff_channel_size = zdim * slice_size;

	for (std::ptrdiff_t z = 0; z < zdim; ++z)
		for (std::ptrdiff_t y = 0; y < ydim; ++y)
			for (std::ptrdiff_t x = 0; x < xdim; ++x) {

				std::size_t idx = z * slice_size + y * xdim + x;
				ID id1 = seg_data[idx];
				statisticsProvider.addVoxel(id1, x, y, z);

				// d=0: z-affinity, neighbor at z-1
				if (z > 0) {
					ID id2 = seg_data[idx - slice_size];
					if (id1 != id2) {
						EdgeIdType e = rg.findEdge(id1, id2);
						if (e == RegionGraphType::NoEdge) {
							e = rg.addEdge(id1, id2);
							statisticsProvider.notifyNewEdge(e);
						}
						statisticsProvider.addAffinity(e, aff_data[idx]);
					}
				}

				// d=1: y-affinity, neighbor at y-1
				if (y > 0) {
					ID id2 = seg_data[idx - xdim];
					if (id1 != id2) {
						EdgeIdType e = rg.findEdge(id1, id2);
						if (e == RegionGraphType::NoEdge) {
							e = rg.addEdge(id1, id2);
							statisticsProvider.notifyNewEdge(e);
						}
						statisticsProvider.addAffinity(e, aff_data[aff_channel_size + idx]);
					}
				}

				// d=2: x-affinity, neighbor at x-1
				if (x > 0) {
					ID id2 = seg_data[idx - 1];
					if (id1 != id2) {
						EdgeIdType e = rg.findEdge(id1, id2);
						if (e == RegionGraphType::NoEdge) {
							e = rg.addEdge(id1, id2);
							statisticsProvider.notifyNewEdge(e);
						}
						statisticsProvider.addAffinity(e, aff_data[2 * aff_channel_size + idx]);
					}
				}
			}

	std::cout << "Region graph number of edges: " << rg.edges().size() << std::endl;
}

template<typename ID, typename StatisticsProviderType>
inline
void
initialize_with_region_graph(
		StatisticsProviderType& statisticsProvider,
		RegionGraph<ID>& rg,
		const std::vector<ScoredEdge>& edges,
		const std::vector<double>& edges_metadata) {

	typedef RegionGraph<ID> RegionGraphType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;

	for (int i = 0; i < edges.size(); ++i) {
		const auto& edge = edges[i];
		double size = edges_metadata[i];
		auto aff = 1.0 - edge.score;
		EdgeIdType e = rg.addEdge(edge.u, edge.v);
		statisticsProvider.addEdge(e, aff, size);
	}
}
