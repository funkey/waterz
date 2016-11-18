#include <memory>

#include <iostream>
#include <algorithm>
#include <vector>

#include "c_frontend.h"
#include "evaluate.hpp"
#include "backend/IterativeRegionMerging.hpp"
#include "backend/MergeFunctions.hpp"
#include "backend/basic_watershed.hpp"
#include "backend/region_graph.hpp"

using namespace std;

// these values based on 5% at iter = 10000
double LOW=  .0001;
double HIGH= .9999;

typedef RegionGraph<uint64_t> RegionGraphType;

std::vector<Metrics> process_thresholds(
		const std::vector<float>& thresholds,
		size_t width, size_t height, size_t depth,
		const float* affinity_data,
		const std::vector<uint64_t*>& segmentation_data,
		const uint32_t* ground_truth_data) {

	size_t num_voxels = width*height*depth;

	assert(thresholds.size() == segmentation_data.size());

	ZwatershedState state = get_initial_state(
			width, height, depth,
			affinity_data,
			segmentation_data[0]);

	std::vector<Metrics> threshold_metrics;

	IterativeRegionMerging<uint64_t, float> regionMerging(*state.region_graph);

	//MedianAffinity<uint64_t, float> mergeFunctionAffinities(*state.region_graph, *state.edge_affinities);

	MaxAffinity<RegionGraphType::EdgeMap<float>> mergeFunctionAffinities(*state.edge_affinities);
	MinSize<RegionGraphType::NodeMap<std::size_t>> mergeFunctionRegionSize(*state.region_sizes);

	//auto mergeFunction = oneMinus(square(mergeFunctionAffinities));
	//auto mergeFunction = divide(
			//mergeFunctionRegionSize,
			//square(
					//mergeFunctionAffinities
			//)
	//);
	//auto mergeFunction = oneMinus(mergeFunctionAffinities);
	auto mergeFunction = multiply(
			oneMinus(
					mergeFunctionAffinities
			),
			mergeFunctionRegionSize
	);

	for (int i = 0; i < thresholds.size(); i++) {

		float threshold = thresholds[i];

		std::cout << "merging until threshold " << threshold << std::endl;
		regionMerging.mergeUntil(
				mergeFunction,
				threshold);

		std::cout << "extracting segmentation" << std::endl;

		// wrap segmentation for current iteration (no copy)
		volume_ref<uint64_t> current_segmentation(
				segmentation_data[i],
				boost::extents[width][height][depth]
		);

		regionMerging.extractSegmentation(current_segmentation);

		// make a copy of the current segmentation for the next iteration
		if (i < segmentation_data.size() - 1)
			std::copy(segmentation_data[i], segmentation_data[i] + num_voxels, segmentation_data[i+1]);

		if (ground_truth_data != 0) {

			std::cout << "evaluating current segmentation against ground-truth" << std::endl;

			// wrap ground-truth (no copy)
			volume_const_ref<uint32_t> ground_truth(
					ground_truth_data,
					boost::extents[width][height][depth]
			);

			auto m = compare_volumes(ground_truth, current_segmentation, width, height, depth);
			Metrics metrics;
			metrics.rand_split = std::get<0>(m);
			metrics.rand_merge = std::get<1>(m);
			metrics.voi_split  = std::get<2>(m);
			metrics.voi_merge  = std::get<3>(m);

			threshold_metrics.push_back(metrics);
		}
	}

	return threshold_metrics;
}

ZwatershedState get_initial_state(
		size_t width, size_t height, size_t depth,
		const float* affinity_data,
		uint64_t* segmentation_data) {

	size_t num_voxels = width*height*depth;

	// wrap affinities (no copy)
	affinity_graph_ref<float> affinities(
			affinity_data,
			boost::extents[3][width][height][depth]
	);

	// wrap segmentation array (no copy)
	volume_ref_ptr<uint64_t> segmentation(
			new volume_ref<uint64_t>(
					segmentation_data,
					boost::extents[width][height][depth]
			)
	);

	std::cout << "performing initial watershed segmentation..." << std::endl;

	counts_t<size_t> counts;
	watershed(affinities, LOW, HIGH, *segmentation, counts);

	std::size_t numNodes = counts.size();

	std::cout << "creating region graph for " << numNodes << " nodes" << std::endl;

	std::shared_ptr<RegionGraphType> regionGraph(
			new RegionGraphType(numNodes)
	);

	std::cout << "creating edge affinity map" << std::endl;

	std::shared_ptr<RegionGraphType::EdgeMap<float>> edgeAffinities(
			new RegionGraphType::EdgeMap<float>(*regionGraph)
	);

	std::cout << "creating region size map" << std::endl;

	// create region size node map, desctruct counts
	std::shared_ptr<RegionGraphType::NodeMap<size_t>> regionSizes(
			new RegionGraphType::NodeMap<size_t>(*regionGraph, std::move(counts))
	);

	std::cout << "extracting region graph..." << std::endl;

	get_region_graph(
			affinities,
			*segmentation,
			numNodes - 1,
			*regionGraph,
			*edgeAffinities);

	ZwatershedState initial_state;
	initial_state.region_graph = regionGraph;
	initial_state.edge_affinities = edgeAffinities;
	initial_state.segmentation = segmentation;
	initial_state.region_sizes = regionSizes;

	return initial_state;
}

