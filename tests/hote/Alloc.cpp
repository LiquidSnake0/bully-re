#include <cstdlib>
#include <cstddef>
void *GameMalloc(size_t size) { return calloc(1, size); }
