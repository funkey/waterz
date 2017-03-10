#pragma once

#include "types.hpp"

#include <cstddef>
#include <iostream>
#include <map>

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

	std::vector<std::map<ID, EdgeIdType>> edges(max_segid+1);

	EdgeIdType e;
	for ( std::ptrdiff_t z = 0; z < zdim; ++z )
		for ( std::ptrdiff_t y = 0; y < ydim; ++y )
			for ( std::ptrdiff_t x = 0; x < xdim; ++x ) {

				statisticsProvider.addVoxel(seg[z][y][x], x, y, z);

				if ((z > 0) && seg[z][y][x] != seg[z-1][y][x]) {

					auto mm = std::minmax(seg[z][y][x], seg[z-1][y][x]);

					auto edgeIt = edges[mm.first].find(mm.second);
					if (edgeIt == edges[mm.first].end()) {
						e = rg.addEdge(mm.first, mm.second);
						edges[mm.first][mm.second] = e;
						statisticsProvider.notifyNewEdge(e);
					} else {
						e = edgeIt->second;
					}

					statisticsProvider.addAffinity(e, aff[0][z][y][x]);
				}

				if ((y > 0) && seg[z][y][x] != seg[z][y-1][x]) {

					auto mm = std::minmax(seg[z][y][x], seg[z][y-1][x]);

					auto edgeIt = edges[mm.first].find(mm.second);
					if (edgeIt == edges[mm.first].end()) {
						e = rg.addEdge(mm.first, mm.second);
						edges[mm.first][mm.second] = e;
						statisticsProvider.notifyNewEdge(e);
					} else {
						e = edgeIt->second;
					}

					statisticsProvider.addAffinity(e, aff[1][z][y][x]);
				}

				if ((x > 0) && seg[z][y][x] != seg[z][y][x-1]) {

					auto mm = std::minmax(seg[z][y][x], seg[z][y][x-1]);

					auto edgeIt = edges[mm.first].find(mm.second);
					if (edgeIt == edges[mm.first].end()) {
						e = rg.addEdge(mm.first, mm.second);
						edges[mm.first][mm.second] = e;
						statisticsProvider.notifyNewEdge(e);
					} else {
						e = edgeIt->second;
					}

					statisticsProvider.addAffinity(e, aff[2][z][y][x]);
				}
			}

	std::cout << "Region graph number of edges: " << rg.edges().size() << std::endl;
}
