
DEFAULT_BENCHMARKS = integerSort/parallelRadixSort comparisonSort/sampleSort comparisonSort/serialSort removeDuplicates/serial_hash removeDuplicates/parlayhash histogram/parallel histogram/sequential wordCounts/histogram wordCounts/serial invertedIndex/sequential invertedIndex/parallel suffixArray/parallelRange suffixArray/serialDivsufsort longestRepeatedSubstring/doubling classify/decisionTree minSpanningForest/parallelFilterKruskal minSpanningForest/serialMST spanningForest/ndST spanningForest/serialST breadthFirstSearch/backForwardBFS breadthFirstSearch/serialBFS maximalMatching/serialMatching maximalMatching/incrementalMatching maximalIndependentSet/ndMIS maximalIndependentSet/serialMIS nearestNeighbors/octTree rayCast/kdTree convexHull/quickHull convexHull/serialHull delaunayTriangulation/incrementalDelaunay delaunayRefine/incrementalRefine rangeQuery2d/parallelPlaneSweep rangeQuery2d/serial nBody/parallelCK

EXT_BENCHMARKS = comparisonSort/quickSort comparisonSort/mergeSort comparisonSort/stableSampleSort removeDuplicates/serial_sort suffixArray/parallelKS spanningForest/incrementalST breadthFirstSearch/simpleBFS breadthFirstSearch/deterministicBFS maximalIndependentSet/incrementalMIS 

SERIAL_VLG_BENCHMARKS = convexHull/serialHull delaunayTriangulation/incrementalDelaunay maximalIndependentSet/serialMIS nBody/parallelCK spanningForest/serialST

ALL_BENCHMARKS = $(DEFAULT_BENCHMARKS) $(EXT_BENCHMARKS)

DATA_GENERATORS = sequenceData graphData geometryData

all : $(SERIAL_VLG_BENCHMARKS) # $(DEFAULT_BENCHMARKS)

ext : $(ALL_BENCHMARKS)

$(SERIAL_VLG_BENCHMARKS) : FORCE
	cd benchmarks/$@; make

# $(DEFAULT_BENCHMARKS) : FORCE
# 	cd benchmarks/$@; make

# $(EXT_BENCHMARKS) : FORCE
# 	cd benchmarks/$@; make

$(DATA_GENERATORS) : FORCE
	cd testData/$@; make -j -s

FORCE :

clean : FORCE
	for bench in $(ALL_BENCHMARKS); do \
	  make clean -s -C benchmarks/$$bench ; \
	done
	for data in $(DATA_GENERATORS); do \
	  make clean -s -C testData/$$data ; \
	  make clean -s -C testData/$$data/data ; \
	done
