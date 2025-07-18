#ifndef MEDIAN_HPP_
#define MEDIAN_HPP_

#include <commons.pc.h>

template<class T>
class Median {
public:
    T getValue() const {
        if (minHeap.empty() && maxHeap.empty())
            throw std::runtime_error("Median of an empty stack is undefined.");
        if (heapSizeOdd())
            return maxHeap.top();          // the middle element
        return maxHeap.top();              // lower median for even size
    }
    
    void addNumber(T number) {
        if (maxHeap.empty() || number <= maxHeap.top()) {
            maxHeap.push(number);
        } else {
            minHeap.push(number);
        }

        if (maxHeap.size() > minHeap.size() + 1) {
            minHeap.push(maxHeap.top());
            maxHeap.pop();
        } else if (minHeap.size() > maxHeap.size()) {
            maxHeap.push(minHeap.top());
            minHeap.pop();
        }
    }
    
    friend std::ostream& operator<<(std::ostream &os, const Median &median) {
        return os << "\t" << "Median: " << median.getValue();
    }
    
    std::size_t getHeapSize() const {
        return minHeap.size() + maxHeap.size();
    }
    
    std::string toStr() const {
        return "median<"+cmn::Meta::toStr(getValue())+">";
    }
    
    bool empty() const noexcept {
        return getHeapSize() == 0;
    }
    
private:
    std::size_t getMaxHeapSize() const {
        return maxHeap.size();
    }
    
    std::size_t getMinHeapSize() const {
        return minHeap.size();
    }
    
    bool heapSizeOdd() const {
        return getHeapSize() % 2;
    }
    
    std::priority_queue<T, std::vector<T>, std::greater<T>> minHeap; // min-heap
    std::priority_queue<T> maxHeap; // max-heap
};

#endif /* MEDIAN_HPP_ */
