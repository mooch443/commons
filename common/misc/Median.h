#ifndef MEDIAN_HPP_
#define MEDIAN_HPP_

#include <commons.pc.h>

template<class T>
class Median {
public:
    Median() : _added(0) {
    }
    
    T getValue() const {
        if(minHeap.empty() && maxHeap.empty())
            return T(INFINITY);
        else if(minHeap.empty() && !maxHeap.empty())
            return maxHeap.top();
        else if(maxHeap.empty() && !minHeap.empty())
            return minHeap.top();
        
        if (heapSizeOdd()) {
            return maxHeap.top();
        } else {
            return (minHeap.top() + maxHeap.top()) / 2.f;
        }
    }
    
    void addNumber(T number) {
        _added++;
        
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
    
protected:
    GETTER(size_t, added);
};

#endif /* MEDIAN_HPP_ */
