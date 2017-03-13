#ifndef WATERZ_STATISTICS_PROVIDER_H__
#define WATERZ_STATISTICS_PROVIDER_H__

/**
 * Base class for statistics providers with fallback implementations.
 */
class StatisticsProvider {

public:

	template <typename EdgeIdType>
	void notifyNewEdge(EdgeIdType e) {}

	template <typename EdgeIdType, typename ScoreType>
	void addAffinity(EdgeIdType e, ScoreType affinity) {}

	template <typename NodeIdType>
	void addVoxel(NodeIdType n, std::size_t x, std::size_t y, std::size_t z) {}

	/**
	 * Callback for node merges: 'from' will be merged into 'to'. Return true, 
	 * if this changed the statistics of this provider.
	 */
	template<typename NodeIdType>
	bool notifyNodeMerge(NodeIdType from, NodeIdType to) { return false; }

	/**
	 * Callback for edge merges: 'from' will be merged into 'to'. Return true, 
	 * if this changed the statistics of this provider.
	 */
	template<typename EdgeIdType>
	bool notifyEdgeMerge(EdgeIdType from, EdgeIdType to) { return false; }
};

#endif // WATERZ_STATISTICS_PROVIDER_H__

