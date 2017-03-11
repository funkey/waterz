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
 * @param region_graph [out]
 *              A reference to a region graph to store the result.
 */
template<typename AG, typename V>
inline
void
get_region_graph( const AG& aff,
                  const V& seg,
                  std::size_t max_segid,
                  RegionGraph<typename V::element>& rg,
				  typename RegionGraph<typename V::element>::template EdgeMap<typename AG::element>& affMap)
{
    typedef typename AG::element        F;
    typedef typename V::element         ID;

    std::ptrdiff_t zdim = aff.shape()[1];
    std::ptrdiff_t ydim = aff.shape()[2];
    std::ptrdiff_t xdim = aff.shape()[3];

    std::vector<std::map<ID,F>> edges(max_segid+1);

    for ( std::ptrdiff_t z = 0; z < zdim; ++z )
        for ( std::ptrdiff_t y = 0; y < ydim; ++y )
            for ( std::ptrdiff_t x = 0; x < xdim; ++x )
            {
                if ( (z > 0) && seg[z][y][x] != seg[z-1][y][x] )
                {
                    auto mm = std::minmax(seg[z][y][x], seg[z-1][y][x]);
                    F& curr = edges[mm.first][mm.second];
                    curr = std::max(curr, aff[0][z][y][x]);
                }
                if ( (y > 0) && seg[z][y][x] != seg[z][y-1][x] )
                {
                    auto mm = std::minmax(seg[z][y][x], seg[z][y-1][x]);
                    F& curr = edges[mm.first][mm.second];
                    curr = std::max(curr, aff[1][z][y][x]);
                }
                if ( (x > 0) && seg[z][y][x] != seg[z][y][x-1] )
                {
                    auto mm = std::minmax(seg[z][y][x], seg[z][y][x-1]);
                    F& curr = edges[mm.first][mm.second];
                    curr = std::max(curr, aff[2][z][y][x]);
                }
            }

    for ( ID id1 = 1; id1 <= max_segid; ++id1 )
    {
        for ( const auto& p: edges[id1] )
        {
			// p.first is ID
			// p.second is F
			auto edgeId = rg.addEdge(id1, p.first);
			affMap[edgeId] = p.second;
			std::cout << id1 << " " << p.first << " = " << p.second << "\n";
        }
    }

    std::cout << "Region graph number of edges: " << rg.edges().size() << std::endl;
}
