#ifndef _AVERAGE_TRACKER_CPP_
#define _AVERAGE_TRACKER_CPP_

#include "AverageTracker.h"

template <typename T>
AverageTracker<T>::AverageTracker( int size )
: BUFFER_LEN(size > 0 ? size : 0), buffer(new T[BUFFER_LEN])
{
  for (int i = 0; i < BUFFER_LEN; i++ )
    buffer[i] = 0;
}

template <typename T>
void AverageTracker<T>::add(T num) {
  sum -= buffer[ next ];
  buffer[ next ] = num;
  sum += num;
  next = (next + 1) % BUFFER_LEN;
}

template <typename T>
int AverageTracker<T>::average() {
  return sum / BUFFER_LEN;
}

template <typename T>
AverageTracker<T>::~AverageTracker() {
  delete[] buffer;
}

#endif
