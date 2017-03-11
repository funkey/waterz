#ifndef C_FRONTEND_H
#define C_FRONTEND_H

#include <vector>

#include "backend/IterativeRegionMerging.hpp"
#include "backend/MergeFunctions.hpp"
#include "backend/Operators.hpp"
#include "backend/types.hpp"
#include "backend/BinQueue.hpp"
#include "backend/PriorityQueue.hpp"
#include "backend/HistogramQuantileProvider.hpp"
#include "backend/VectorQuantileProvider.hpp"
#include "evaluate.hpp"

typedef uint64_t                                                 SegID;
typedef uint32_t                                                 GtID;
typedef float                                                    AffValue;
typedef float                                                    ScoreValue;
typedef RegionGraph<SegID>                                       RegionGraphType;
typedef typename RegionGraphType::template EdgeMap<AffValue>     AffinitiesType;
typedef typename RegionGraphType::template NodeMap<std::size_t>  SizesType;

// to be created by __init__.py
#include <ScoringFunction.h>
#include <Queue.h>

typedef IterativeRegionMerging<SegID, ScoreValue, QueueType>     RegionMergingType;

struct Metrics {

	double voi_split;
	double voi_merge;
	double rand_split;
	double rand_merge;
};

struct Merge {

	SegID a;
	SegID b;
	SegID c;
	ScoreValue score;
};

struct WaterzState {

	int     context;
	Metrics metrics;
};

class WaterzContext {

public:

	static WaterzContext* createNew() {

		WaterzContext* context = new WaterzContext();
		context->id = _nextId;
		_nextId++;
		_contexts.insert(std::make_pair(context->id, context));

		return context;
	}

	static WaterzContext* get(int id) {

		if (!_contexts.count(id))
			return NULL;

		return _contexts.at(id);
	}

	static void free(int id) {

		WaterzContext* context = get(id);

		if (context) {

			_contexts.erase(id);
			delete context;
		}
	}

	int id;

	std::shared_ptr<RegionGraphType>     regionGraph;
	std::shared_ptr<AffinitiesType>      edgeAffinities;
	std::shared_ptr<SizesType>           regionSizes;

	std::shared_ptr<RegionMergingType>   regionMerging;
	std::shared_ptr<ScoringFunctionType> scoringFunction;
	volume_ref_ptr<SegID>                segmentation;
	volume_const_ref_ptr<GtID>           groundtruth;

private:

	WaterzContext() {}

	~WaterzContext() {}

	static std::map<int, WaterzContext*> _contexts;
	static int _nextId;
};

class RegionMergingVisitor {

public:

	void onPop(RegionGraphType::EdgeIdType e, ScoreValue score) {}

	void onDeletedEdgeFound(RegionGraphType::EdgeIdType e) {}

	void onStaleEdgeFound(RegionGraphType::EdgeIdType e, ScoreValue oldScore, ScoreValue newScore) {}

	void onMerge(SegID a, SegID b, SegID c, ScoreValue score) {}
};

class MergeHistoryVisitor : public RegionMergingVisitor {

public:

	MergeHistoryVisitor(std::vector<Merge>& history) : _history(history) {}

	void onMerge(SegID a, SegID b, SegID c, ScoreValue score) {

		_history.push_back({a, b, c, score});
	}

private:

	std::vector<Merge>& _history;
};

class DebugVisitor : public RegionMergingVisitor {

public:

	DebugVisitor(RegionGraphType& regionGraph) : _regionGraph(regionGraph) {}

	void onPop(RegionGraphType::EdgeIdType e, ScoreValue score) {

		std::cout << "poppded edge " << e << " with " << score << std::endl;
	}

	void onDeletedEdgeFound(RegionGraphType::EdgeIdType e) {

		std::cout << "edge " << e << " is marked as deleted" << std::endl;
	}

	void onStaleEdgeFound(RegionGraphType::EdgeIdType e, ScoreValue oldScore, ScoreValue newScore) {

		std::cout << "edge " << e << " is marked as stale: ";
		std::cout << "new score " << newScore << " (was " << oldScore << ")" << std::endl;

		if (newScore < oldScore)
			std::cout << "!!! new score is smaller than old score !!!" << std::endl;
	}

	void onMerge(SegID a, SegID b, SegID c, ScoreValue score) {

		std::cout << "merging region " << a << " + " << b << " -> " << c << std::endl;
	}

	void onEdgeScored(RegionGraphType::EdgeIdType e, ScoreValue score) {

		std::cout << _regionGraph.edge(e).u << " " << _regionGraph.edge(e).v << " = " << score << std::endl;
	}

private:

	RegionGraphType& _regionGraph;
};

WaterzState initialize(
		size_t          width,
		size_t          height,
		size_t          depth,
		const AffValue* affinity_data,
		SegID*          segmentation_data,
		const GtID*     groundtruth_data = NULL,
		AffValue        affThresholdLow  = 0.0001,
		AffValue        affThresholdHigh = 0.9999,
		bool            findFragments = true);

std::vector<Merge> mergeUntil(
		WaterzState& state,
		float        threshold);

void free(WaterzState& state);

#endif
