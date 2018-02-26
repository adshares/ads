#ifndef OSTREAM_H
#define OSTREAM_H

#include <stdio.h>
#include <stdint.h>
#include <ostream>
#include <vector>

template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v);


#endif // OSTREAM_H
