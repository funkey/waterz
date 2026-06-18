#include <memory>

#include <iostream>
#include <algorithm>
#include <vector>

#include "frontend_agglomerate.h"
#include "evaluate.hpp"
#include "backend/MergeFunctions.hpp"
#include "backend/basic_watershed.hpp"
#include "backend/region_graph.hpp"

#include "backend/SemanticConstraintProvider.hpp"
#include "backend/SemanticTaintConstraintProvider.hpp"
#include "backend/SegConstraintProvider.hpp"
#include "backend/SizeHeuristicConstraintProvider.hpp"

std::map<int, WaterzContext*> WaterzContext::_contexts;
int WaterzContext::_nextId = 0;

std::vector<ScoredEdge> getRegionGraph(WaterzState& state);
std::vector<double> getRegionGraphMeta(WaterzState& state);

WaterzState
initialize_with_rag(
		const std::vector<ScoredEdge>& rag,
		const std::vector<double>& rag_metadata,
		SegID* segmentation_data,
		std::size_t     width,
		std::size_t     height,
		std::size_t     depth) {

	SegID maxId = 0;
	for (const auto& edge : rag) {
		maxId = std::max(std::max(edge.u, edge.v), maxId);
	}

	std::size_t numNodes = maxId + 1;
	std::cout << "creating region graph for " << numNodes << " nodes" << std::endl;

	std::shared_ptr<RegionGraphType> regionGraph(
			new RegionGraphType(numNodes)
	);

	std::cout << "creating statistics provider" << std::endl;
	std::shared_ptr<StatisticsProviderType> statisticsProvider(
			new StatisticsProviderType(*regionGraph)
	);

	std::cout << "initializing region graph..." << std::endl;

	initialize_with_region_graph<SegID>(
			*statisticsProvider,
			*regionGraph,
			rag,
			rag_metadata);

	std::shared_ptr<ScoringFunctionType> scoringFunction(
			new ScoringFunctionType(*regionGraph, *statisticsProvider)
	);

	std::shared_ptr<RegionMergingType> regionMerging(
			new RegionMergingType(*regionGraph)
	);

	
	std::shared_ptr<vector<ConstraintProvider*>> dummy_constraints(
		new vector<ConstraintProvider*>()
	);

	WaterzContext* context = WaterzContext::createNew();
	context->regionGraph        = regionGraph;
	context->regionMerging      = regionMerging;
	context->scoringFunction    = scoringFunction;
	context->statisticsProvider = statisticsProvider;
	// context->segmentation       = segmentation_data;
	context->constraints 		= dummy_constraints;

	if (segmentation_data != NULL) {
		// wrap data (no copy)
		volume_ref_ptr<SegID> segmentation(
				new volume_ref<SegID>(
						segmentation_data,
						boost::extents[width][height][depth]
				)
		);
		context->segmentation = segmentation;
	}

	WaterzState initial_state;
	initial_state.context = context->id;

	return initial_state;
}

WaterzState
initialize(
		std::size_t     width,
		std::size_t     height,
		std::size_t     depth,
		const AffValue* affinity_data,
		SegID*          segmentation_data,
		const GtID*     ground_truth_data,
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
		bool            findFragments) {

	std::size_t num_voxels = width*height*depth;

	// wrap affinities (no copy)
	affinity_graph_ref<AffValue> affinities(
			affinity_data,
			boost::extents[3][width][height][depth]
	);

	// wrap segmentation array (no copy)
	volume_ref_ptr<SegID> segmentation(
			new volume_ref<SegID>(
					segmentation_data,
					boost::extents[width][height][depth]
			)
	);

	counts_t<std::size_t> sizes;

	if (findFragments) {
		std::cout << "performing initial watershed segmentation..." << std::endl;
		watershed(affinities, affThresholdLow, affThresholdHigh, *segmentation, sizes);
	} else {
		std::cout << "counting regions and sizes..." << std::endl;
		std::size_t maxId = *std::max_element(segmentation_data, segmentation_data + num_voxels);
		sizes.resize(maxId + 1);
		for (std::size_t i = 0; i < num_voxels; i++)
			sizes[segmentation_data[i]]++;
	}

	std::size_t numNodes = sizes.size();
	std::cout << "creating region graph for " << numNodes << " nodes" << std::endl;

	std::shared_ptr<RegionGraphType> regionGraph(
			new RegionGraphType(numNodes)
	);

	std::cout << "creating statistics provider" << std::endl;
	std::shared_ptr<StatisticsProviderType> statisticsProvider(
			new StatisticsProviderType(*regionGraph)
	);

	std::cout << "extracting region graph..." << std::endl;

	get_region_graph(
			affinities,
			*segmentation,
			numNodes - 1,
			*statisticsProvider,
			*regionGraph);

	std::shared_ptr<ScoringFunctionType> scoringFunction(
			new ScoringFunctionType(*regionGraph, *statisticsProvider)
	);

	std::shared_ptr<RegionMergingType> regionMerging(
			new RegionMergingType(*regionGraph)
	);

	std::shared_ptr<vector<ConstraintProvider*>> constraints(
		new vector<ConstraintProvider*>()
	);

	if (semantic_data != NULL) {
		std::cout << "getting semantic information..." << std::endl;
		constraints->push_back(
			new SemanticConstraintProvider<RegionGraphType, SemValue, SegID>(
				semantic_data, segmentation_data, num_voxels,
				semantic_aff_threshold,
				semantic_size_threshold,
				semantic_signal_ratio
			)
		);
	}

	if (semantic_data != NULL && !semantic_taint_labels.empty()) {
		std::cout << "getting semantic taint constraint information..." << std::endl;
		constraints->push_back(
			new SemanticTaintConstraintProvider<RegionGraphType, SemValue, SegID>(
				semantic_data, segmentation_data, num_voxels,
				semantic_taint_labels,
				semantic_taint_threshold
			)
		);
	}

	if (segconstraint_data != NULL) {
		std::cout << "getting seg constraint information..." << std::endl;
		constraints->push_back(
			new SegConstraintProvider<RegionGraphType, SegID>(
				segconstraint_data, segmentation_data, num_voxels
			)
		);
	}

	constraints->push_back(
		new SizeHeuristicConstraintProvider<RegionGraphType, SegID>(
			sizes,
			size_heuristic_aff_threshold,
			size_heuristic_small_threshold,
			size_heuristic_large_threshold
		)
	);

	WaterzContext* context = WaterzContext::createNew();
	context->regionGraph        = regionGraph;
	context->regionMerging      = regionMerging;
	context->scoringFunction    = scoringFunction;
	context->statisticsProvider = statisticsProvider;
	context->segmentation       = segmentation;
	context->constraints 		= constraints;

	WaterzState initial_state;
	initial_state.context = context->id;

	if (ground_truth_data != NULL) {

		// wrap ground-truth (no copy)
		volume_const_ref_ptr<GtID> groundtruth(
				new volume_const_ref<GtID>(
						ground_truth_data,
						boost::extents[width][height][depth]
				)
		);

		context->groundtruth = groundtruth;
	}

	return initial_state;
}

std::vector<Merge>
mergeUntil(
		WaterzState& state,
		float        threshold) {

	WaterzContext* context = WaterzContext::get(state.context);

	std::cout << "merging until threshold " << threshold << std::endl;

	std::vector<Merge>  mergeHistory;
	MergeHistoryVisitor mergeHistoryVisitor(mergeHistory);

	std::size_t merged = context->regionMerging->mergeUntil(
			*context->scoringFunction,
			*context->statisticsProvider,
			*context->constraints,
			threshold,
			mergeHistoryVisitor
		);

	if (merged && context->segmentation) {

		std::cout << "extracting segmentation" << std::endl;

		context->regionMerging->extractSegmentation(*context->segmentation);
	}

	if (context->groundtruth) {

		std::cout << "evaluating current segmentation against ground-truth" << std::endl;

		auto m = compare_volumes(*context->groundtruth, *context->segmentation);

		state.metrics.rand_split = std::get<0>(m);
		state.metrics.rand_merge = std::get<1>(m);
		state.metrics.voi_split  = std::get<2>(m);
		state.metrics.voi_merge  = std::get<3>(m);
	}

	return mergeHistory;
}

std::vector<ScoredEdge>
getRegionGraph(WaterzState& state) {

	WaterzContext* context = WaterzContext::get(state.context);
	std::shared_ptr<RegionMergingType> regionMerging = context->regionMerging;
	std::shared_ptr<ScoringFunctionType> scoringFunction = context->scoringFunction;

	return regionMerging->extractRegionGraph<ScoredEdge>(*scoringFunction);
}

std::vector<double>
getRegionGraphMeta(WaterzState& state) {

	WaterzContext* context = WaterzContext::get(state.context);
	std::shared_ptr<RegionMergingType> regionMerging = context->regionMerging;
	std::shared_ptr<ScoringFunctionType> scoringFunction = context->scoringFunction;

	return regionMerging->extractRegionGraphMeta<double>(*scoringFunction, *(context->statisticsProvider));
}

void
free(WaterzState& state) {

	WaterzContext::free(state.context);
}
