#ifndef _AVERAGE_TRACKER_H_
#define _AVERAGE_TRACKER_H_

template <typename T>
class AverageTracker {
public:
  const int BUFFER_LEN;
  
  AverageTracker( int size );
  void add(T num);
  int average();
  ~AverageTracker();
  
private:
  T* buffer;
  int next = 0;
  int sum = 0;
};

#include "AverageTracker.cpp"

#endif
