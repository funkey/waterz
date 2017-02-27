#ifndef WATERZ_BIN_QUEUE_H__
#define WATERZ_BIN_QUEUE_H__

#include <queue>

/**
 * A priority queue sorting elements from smallest to largest given a discrete 
 * score between 0 and N.
 */
template <typename T, int N=256>
class BinQueue {

public:

	BinQueue() :
		_minBin(-1) {}

	void push(const T& element, int score) {

		_bins[score].push(element);
		if (_minBin == -1)
			_minBin = score;
		else
			_minBin = std::min(score, _minBin);
	}

	const T& top() {

		return _bins[_minBin].front();
	}

	void pop() {

		_bins[_minBin].pop();

		if (_bins[_minBin].empty()) {

			// find next non-empty bin
			for (;_minBin < N; _minBin++)
				if (!_bins[_minBin].empty())
					return;

			// queue is empty
			_minBin = -1;
		}
	}

	bool empty() {

		return (_minBin == -1);
	}

	/**
	 * Get the sore of the smallest element in the queue, or -1 if the queue is 
	 * empty.
	 */
	int minScore() {

		return _minBin;
	}

private:

	std::queue<T> _bins[N];

	// smallest non-empty bin
	int _minBin;
};

#endif // WATERZ_BIN_QUEUE_H__

