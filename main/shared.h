#ifndef SHARED_H
#define SHARED_H

#define defer(start, end) for (    \
  int (_i_ ## __LINE__) = (start, 0); \
  !(_i_ ## __LINE__);                 \
  ((_i_ ## __LINE__) += 1), end)

typedef struct str {
  int length;
  char* data;
} str;

#endif // SHARED_H
