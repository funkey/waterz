#ifndef ITERATIVE_REGION_MERGING_H__
#define ITERATIVE_REGION_MERGING_H__

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <queue>
#include <cassert>
#include <limits>

#include "RegionGraph.hpp"
#include "PriorityQueue.hpp"
#include "ConstraintProvider.hpp"

template <typename NodeIdType, typename ScoreType, template <typename T, typename S> class QueueType = PriorityQueue>
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
		_mergedUntil(std::numeric_limits<ScoreType>::lowest()) {}

	/**
	 * Merge a RAG with the given edge scoring function until the given threshold.
	 */
	template <typename EdgeScoringFunction, typename StatisticsProviderType, typename Visitor>
	std::size_t mergeUntil(
			EdgeScoringFunction& edgeScoringFunction,
			StatisticsProviderType& statisticsProvider,
			std::vector<ConstraintProvider*>& constraints,
			ScoreType threshold,
			Visitor& visitor) {

		if (threshold <= _mergedUntil) {

			std::cout << "already merged until " << threshold << ", skipping" << std::endl;
			return 0;
		}

		// compute scores of each edge not scored so far
		if (_mergedUntil == std::numeric_limits<ScoreType>::lowest()) {

			std::cout << "computing initial scores" << std::endl;

			for (EdgeIdType e = 0; e < _regionGraph.edges().size(); e++)
				scoreEdge(e, edgeScoringFunction);
		}

		std::cout << "merging until " << threshold << std::endl;

		if (!_edgeQueue.empty())
			std::cout << "min edge score " << _edgeScores[_edgeQueue.top()] << std::endl;

		// while there are still unhandled edges
		std::size_t merged = 0;
		while (!_edgeQueue.empty()) {

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

			visitor.onPop(next, score);

			if (_deleted[next]) {

				visitor.onDeletedEdgeFound(next);
				continue;
			}

			if (_stale[next]) {

				// if we encountered a stale edge, recompute it's score and
				// place it back in the queue
				ScoreType newScore = scoreEdge(next, edgeScoringFunction);
				_stale[next] = false;
				// A stale edge's score is recomputed from running/area-weighted
				// mean affinities in floating point, so it can dip ~1 ULP below
				// the score it was queued at. Tolerate that rounding noise; a
				// real non-monotonicity would exceed this by orders of magnitude.
				assert(newScore >= score - 1e-5);

				visitor.onStaleEdgeFound(next, score, newScore);

				continue;
			}

			NodeIdType a = _regionGraph.edge(next).u;
			NodeIdType b = _regionGraph.edge(next).v;

			bool is_constrained = false;
			for (auto constraint : constraints) {
				is_constrained |= constraint->isConstrained(a, b, score);
			}

			if (is_constrained) {
				continue;  // skip merging
			}

			NodeIdType newRegion = mergeRegions(next, statisticsProvider, constraints);
			merged++;

			visitor.onMerge(
					a, b,
					newRegion,
					score);
		}

		std::cout << "merged " << merged << " edges" << std::endl;

		_mergedUntil = threshold;

		return merged;
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

	/**
	 * Get the region graph corresponding to the current merge level.
	 */
	template <typename ScoredEdge, typename EdgeScoringFunction>
	std::vector<ScoredEdge> extractRegionGraph(EdgeScoringFunction& edgeScoringFunction) {

		std::vector<ScoredEdge> edges;

		for (EdgeIdType e = 0; e < _regionGraph.numEdges(); e++) {

			if (_deleted[e])
				continue;

			ScoreType score;
			if (_stale[e])
				score = scoreEdge(e, edgeScoringFunction);
			else
				score = _edgeScores[e];

			if (score < _mergedUntil)
				continue;

			edges.push_back(
				ScoredEdge(
					_regionGraph.edge(e).u,
					_regionGraph.edge(e).v,
					score));
		}

		return edges;
	}

	template <typename EdgeMetadata, typename EdgeScoringFunction, typename StatisticsProviderType>
	std::vector<EdgeMetadata> extractRegionGraphMeta(EdgeScoringFunction& edgeScoringFunction, StatisticsProviderType& statisticsProvider) {

		std::vector<EdgeMetadata> ret;

		for (EdgeIdType e = 0; e < _regionGraph.numEdges(); e++) {

			if (_deleted[e])
				continue;

			ScoreType score;
			if (_stale[e])
				score = scoreEdge(e, edgeScoringFunction);
			else
				score = _edgeScores[e];

			if (score < _mergedUntil)
				continue;

			ret.emplace_back(statisticsProvider.getEdgeMetadata(e));
		}

		return ret;
	}

private:

	/**
	 * Merge regions a and b.
	 */
	template <typename StatisticsProviderType>
	NodeIdType mergeRegions(
			EdgeIdType e,
			StatisticsProviderType& statisticsProvider,
			std::vector<ConstraintProvider*>& constraints) {

		NodeIdType a = _regionGraph.edge(e).u;
		NodeIdType b = _regionGraph.edge(e).v;

		// assign new node a = a + b
		bool nodeStatisticsChanged = statisticsProvider.notifyNodeMerge(b, a);

		for (auto constraint : constraints) {
			nodeStatisticsChanged |= constraint->notifyNodeMerge(b, a);
		}

		// set path
		_rootPaths[b] = a;

		if (nodeStatisticsChanged) {

			// mark all incident edges of a as stale...
			for (EdgeIdType neighborEdge : _regionGraph.incEdges(a))
				_stale[neighborEdge] = true;
		}

		// ...and update incident edges of b
		// Take b's incident list via swap to avoid copying the vector.
		// b's list is cleared, so use reassignEdge/removeEdgeSkipNode
		// which don't touch b's incEdges.
		std::vector<EdgeIdType> neighborEdges = _regionGraph.takeIncEdges(b);
		for (EdgeIdType neighborEdge : neighborEdges) {

			if (neighborEdge == e)
				continue;

			NodeIdType neighbor = _regionGraph.getOpposite(b, neighborEdge);

			// There are two kinds of neighbors of b:
			//
			//   1. exclusive to b
			//   2. shared by a and b

			EdgeIdType aNeighborEdge = _regionGraph.findEdge(a, neighbor);

			if (aNeighborEdge == RegionGraphType::NoEdge) {

				// We encountered an exclusive neighbor of b.

				_regionGraph.reassignEdge(neighborEdge, b, a);

				if (nodeStatisticsChanged)
					_stale[neighborEdge] = true;

			} else {

				// We encountered a shared neighbor. We have to:
				//
				// * merge the more expensive edge one into the cheaper one
				// * mark the cheaper one as stale (if it isn't already)
				// * delete the more expensive one
				//
				// This ensures that the stale edge bubbles up early enough
				// to consider it's real score (which is assumed to be
				// larger than the minium of the two original scores).

				if (_edgeScores[neighborEdge] > _edgeScores[aNeighborEdge]) {

					// We got lucky, we can reuse the edge that is attached to a
					// already

					bool edgeStatisticChanged = statisticsProvider.notifyEdgeMerge(neighborEdge, aNeighborEdge);

					_regionGraph.removeEdgeSkipNode(neighborEdge, b);
					_deleted[neighborEdge] = true;
					if (edgeStatisticChanged)
						_stale[aNeighborEdge] = true;

				} else {

					// Bummer. The new edge should be the one pointing from
					// a to neighbor.

					bool edgeStatisticChanged = statisticsProvider.notifyEdgeMerge(aNeighborEdge, neighborEdge);

					_regionGraph.replaceEdge(aNeighborEdge, neighborEdge, a, neighbor, b);

					if (edgeStatisticChanged)
						_stale[neighborEdge] = true;
					_deleted[aNeighborEdge] = true;
				}
			}
		}

		// the new node
		return a;
	}

	/**
	 * Score edge e.
	 */
	template <typename EdgeScoringFunction>
	ScoreType scoreEdge(EdgeIdType e, EdgeScoringFunction& edgeScoringFunction) {

		ScoreType score = edgeScoringFunction(e);

		_edgeScores[e] = score;
		_edgeQueue.push(e, score);

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
	QueueType<EdgeIdType, ScoreType> _edgeQueue;

	// paths from nodes to the roots of the merge-tree they are part of
	//
	// root nodes are not in the map
	//
	// paths will be compressed when read
	std::unordered_map<NodeIdType, NodeIdType> _rootPaths;

	// current state of merging
	ScoreType _mergedUntil;
};

#endif // ITERATIVE_REGION_MERGING_H__

