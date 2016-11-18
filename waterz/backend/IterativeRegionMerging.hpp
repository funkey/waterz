#ifndef ITERATIVE_REGION_MERGING_H__
#define ITERATIVE_REGION_MERGING_H__

#include <vector>
#include <map>
#include <queue>
#include <cassert>

#include "RegionGraph.hpp"

template <typename NodeIdType, typename ScoreType>
class IterativeRegionMerging {

public:

	typedef RegionGraph<NodeIdType>              RegionGraphType;
	typedef typename RegionGraphType::EdgeType   EdgeType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;

	/**
	 * Create a region merging for the given initial RAG.
	 */
	IterativeRegionMerging(RegionGraphType& initialRegionGraph) :
		_regionGraph(initialRegionGraph),
		_edgeScores(initialRegionGraph),
		_deleted(initialRegionGraph),
		_stale(initialRegionGraph),
		_edgeQueue(EdgeCompare(_edgeScores)),
		_mergedUntil(0) {}

	/**
	 * Merge a RAG with the given edge scoring function until the given threshold.
	 */
	template <typename EdgeScoringFunction>
	void mergeUntil(
			EdgeScoringFunction& edgeScoringFunction,
			ScoreType threshold) {

		if (threshold <= _mergedUntil) {

			std::cout << "already merged until " << threshold << ", skipping" << std::endl;
			return;
		}

		// compute scores of each edge not scored so far
		if (_mergedUntil == 0) {

			std::cout << "computing initial scores" << std::endl;

			for (EdgeIdType e = 0; e < _regionGraph.edges().size(); e++)
				scoreEdge(e, edgeScoringFunction);
		}

		std::cout << "merging until " << threshold << std::endl;

		// while there are still unhandled edges
		while (_edgeQueue.size() > 0) {

			// get the next cheapest edge to merge
			EdgeIdType next = _edgeQueue.top();
			ScoreType score = _edgeScores[next];

			// stop, if the threshold got exceeded
			// (also if edge is stale or got deleted, as new edges can only be 
			// more expensive)
			if (score >= threshold) {

				std::cout << "threshold exceeded" << std::endl;
				break;
			}

			_edgeQueue.pop();

			if (_deleted[next])
				continue;

			if (_stale[next]) {

				// if we encountered a stale edge, recompute it's score and 
				// place it back in the queue
				ScoreType newScore = scoreEdge(next, edgeScoringFunction);
				_stale[next] = false;
				assert(newScore >= score);

				continue;
			}

			mergeRegions(next, edgeScoringFunction);
		}

		_mergedUntil = threshold;
	}

	/**
	 * Get the segmentation corresponding to the current merge level.
	 *
	 * The provided segmentation has to hold the initial segmentation, or any 
	 * segmentation created by previous calls to extractSegmentation(). In other 
	 * words, it has to hold IDs that have been seen before.
	 */
	template <typename SegmentationVolume>
	void extractSegmentation(SegmentationVolume& segmentation) {

		for (std::size_t i = 0; i < segmentation.num_elements(); i++)
			segmentation.data()[i] = getRoot(segmentation.data()[i]);
	}

private:

	/**
	 * Compare two edges based on their score. To be used in the priority queue.
	 */
	class EdgeCompare {

	public:

		EdgeCompare(const typename RegionGraphType::template EdgeMap<ScoreType>& edgeScores) :
			_edgeScores(edgeScores) {}

		bool operator()(EdgeIdType a, EdgeIdType b) {

			if (_edgeScores[a] == _edgeScores[b])
				return a > b;

			return _edgeScores[a] > _edgeScores[b];
		}

	private:

		const typename RegionGraphType::template EdgeMap<ScoreType>& _edgeScores;
	};

	/**
	 * Merge regions a and b.
	 */
	template <typename EdgeScoringFunction>
	void mergeRegions(
			EdgeIdType e,
			EdgeScoringFunction& edgeScoringFunction) {

		NodeIdType a = _regionGraph.edge(e).u;
		NodeIdType b = _regionGraph.edge(e).v;

		// create a new node c = a + b
		// TODO: re-use node a
		NodeIdType c = _regionGraph.addNode();

		edgeScoringFunction.notifyNodeMerge(a, b, c);

		// set parents
		_rootPaths[a] = c;
		_rootPaths[b] = c;

		// connect c to neighbors of a and b, and update affiliatedEdges

		// for each child region
		for (NodeIdType child : { a, b } ) {

			// for all neighbors of child...
			std::vector<EdgeIdType> neighborEdges = _regionGraph.incEdges(child);
			for (EdgeIdType neighborEdge : neighborEdges) {

				// ...except the edge we merge already
				if (neighborEdge == e)
					continue;

				NodeIdType neighbor = _regionGraph.getOpposite(child, neighborEdge);

				// There are two kinds of neighbors:
				//
				//   1. exclusive to a or b
				//   2. shared by a and b
				//
				// That means we encounter edges to neighbors either one or two 
				// times.

				EdgeIdType prevNeighborEdge = _regionGraph.findEdge(c, neighbor);

				if (prevNeighborEdge == RegionGraphType::NoEdge) {

					// We encountered the first edge to neighbor. Following an 
					// optimisitic strategy, we move it to point from c to 
					// neighbor and mark it as stale. If this was the only edge 
					// to neighbor, we are done.

					_regionGraph.moveEdge(neighborEdge, c, neighbor);
					assert(_regionGraph.findEdge(c, neighbor) == neighborEdge);
					_stale[neighborEdge] = true;

				} else {

					// We encountered the second edge to neighbor. We have to:
					//
					// * merge the more expensive one into the cheaper one
					// * mark the cheaper one as stale (if it isn't already)
					// * delete the more expensive one
					//
					// This ensures that the stale edge bubbles up early enough 
					// to consider it's real score (which is assumed to be 
					// larger than the minium of the two original scores).

					if (_edgeScores[neighborEdge] > _edgeScores[prevNeighborEdge]) {

						// We got lucky, we can reuse the edge we moved already.

						edgeScoringFunction.notifyEdgeMerge(neighborEdge, prevNeighborEdge);
						_deleted[neighborEdge] = true;

					} else {

						// Bummer. The new edge should be the one pointing from 
						// c to neighbor.

						edgeScoringFunction.notifyEdgeMerge(prevNeighborEdge, neighborEdge);

						_regionGraph.removeEdge(prevNeighborEdge);
						_regionGraph.moveEdge(neighborEdge, c, neighbor);
						assert(_regionGraph.findEdge(c, neighbor) == neighborEdge);

						_stale[neighborEdge] = true;
						_deleted[prevNeighborEdge] = true;
					}
				}
			}
		}
	}

	/**
	 * Score edge e.
	 */
	template <typename EdgeScoringFunction>
	ScoreType scoreEdge(EdgeIdType e, EdgeScoringFunction& edgeScoringFunction) {

		ScoreType score = edgeScoringFunction(e);

		_edgeScores[e] = score;
		_edgeQueue.push(e);

		return score;
	}

	inline bool isRoot(NodeIdType id) {

		// if there is no root path, it is a root
		return (_rootPaths.count(id) == 0);
	}

	/**
	 * Get the root node of a merge-tree.
	 */
	NodeIdType getRoot(NodeIdType id) {

		// early way out
		if (isRoot(id))
			return id;

		// walk up to root

		NodeIdType root = _rootPaths.at(id);
		while (!isRoot(root))
			root = _rootPaths.at(root);

		// not compressed, yet
		if (_rootPaths.at(id) != root)
			while (id != root) {

				NodeIdType next = _rootPaths.at(id);
				_rootPaths[id] = root;
				id = next;
			}

		return root;
	}

	RegionGraphType& _regionGraph;

	// the score of each edge
	typename RegionGraphType::template EdgeMap<ScoreType> _edgeScores;

	typename RegionGraphType::template EdgeMap<bool> _stale;
	typename RegionGraphType::template EdgeMap<bool> _deleted;

	// sorted list of edges indices, cheapest edge first
	std::priority_queue<EdgeIdType, std::vector<EdgeIdType>, EdgeCompare> _edgeQueue;

	// paths from nodes to the roots of the merge-tree they are part of
	//
	// root nodes are not in the map
	//
	// paths will be compressed when read
	std::map<NodeIdType, NodeIdType> _rootPaths;

	// current state of merging
	ScoreType _mergedUntil;
};

#endif // ITERATIVE_REGION_MERGING_H__

