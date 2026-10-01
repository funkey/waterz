// The frontend for one scoring function and queue. This file is included, once
// per variant, in a namespace (see agglomerate.pyx), after
// frontend_agglomerate_types.h, and after these declarations:
//
//   typedef ... ScoringFunctionType;
//   template<typename T, typename S> using QueueType = ...;

typedef typename ScoringFunctionType::StatisticsProviderType StatisticsProviderType;
typedef IterativeRegionMerging<SegID, ScoreValue, QueueType> RegionMergingType;

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
		const GtID*     groundtruth_data = NULL,
		AffValue        affThresholdLow  = 0.0001,
		AffValue        affThresholdHigh = 0.9999,
		bool            findFragments = true);

std::vector<Merge> mergeUntil(
		WaterzState& state,
		float        threshold);

std::vector<ScoredEdge> getRegionGraph(WaterzState& state);

void free(WaterzState& state);
