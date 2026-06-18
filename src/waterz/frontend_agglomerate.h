#ifndef C_FRONTEND_H
#define C_FRONTEND_H

#include <cstdint>
#include <vector>

#include "backend/IterativeRegionMerging.hpp"
#include "backend/MergeFunctions.hpp"
#include "backend/Operators.hpp"
#include "backend/types.hpp"
#include "backend/BinQueue.hpp"
#include "backend/PriorityQueue.hpp"
#include "backend/HistogramQuantileProvider.hpp"
#include "backend/VectorQuantileProvider.hpp"
#include "backend/ConstraintProvider.hpp"
#include "evaluate.hpp"


typedef uint64_t SegID;
typedef uint32_t GtID;
typedef uint8_t SemValue;
typedef float AffValue;
typedef float ScoreValue;
typedef RegionGraph<SegID> RegionGraphType;

// to be created by __init__.py
#include <ScoringFunction.h>
#include <Queue.h>

typedef typename ScoringFunctionType::StatisticsProviderType StatisticsProviderType;
typedef IterativeRegionMerging<SegID, ScoreValue, QueueType> RegionMergingType;

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

struct ScoredEdge {

	ScoredEdge() {};

	ScoredEdge(SegID u_, SegID v_, ScoreValue score_) :
		u(u_),
		v(v_),
		score(score_) {}

	SegID u;
	SegID v;
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

	std::shared_ptr<RegionGraphType> regionGraph;
	std::shared_ptr<RegionMergingType> regionMerging;
	std::shared_ptr<ScoringFunctionType> scoringFunction;
	std::shared_ptr<StatisticsProviderType> statisticsProvider;
	std::shared_ptr<vector<ConstraintProvider*>> constraints;
	volume_ref_ptr<SegID> segmentation;
	volume_const_ref_ptr<GtID> groundtruth;

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

WaterzState initialize(
		size_t          width,
		size_t          height,
		size_t          depth,
		const AffValue* affinity_data,
		SegID*          segmentation_data,
		const GtID*     groundtruth_data,
		const SemValue* semantic_data,
		const SegID*    segconstraint_data,
		AffValue        affThresholdLow,
		AffValue        affThresholdHigh,
		AffValue        semantic_aff_threshold,
		size_t          semantic_size_threshold,
		AffValue        semantic_signal_ratio,
		const std::vector<SemValue>& semantic_taint_labels,
		AffValue        semantic_taint_threshold,
		AffValue        size_heuristic_aff_threshold,
		size_t          size_heuristic_small_threshold,
		size_t          size_heuristic_large_threshold,
		bool            findFragments
	);

WaterzState initialize_with_rag(
		const std::vector<ScoredEdge>& rag,
		const std::vector<double>& rag_metadata,
		SegID*          segmentation_data,
		std::size_t     width,
		std::size_t     height,
		std::size_t     depth);

std::vector<Merge> mergeUntil(
		WaterzState& state,
		float        threshold);

std::vector<ScoredEdge> getRegionGraph(WaterzState& state);
std::vector<double> getRegionGraphMeta(WaterzState& state);

void free(WaterzState& state);

#endif
