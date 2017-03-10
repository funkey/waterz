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

	template<typename NodeIdType>
	void notifyNodeMerge(NodeIdType from, NodeIdType to) {}

	template<typename EdgeIdType>
	void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {}
};

#endif // WATERZ_STATISTICS_PROVIDER_H__

