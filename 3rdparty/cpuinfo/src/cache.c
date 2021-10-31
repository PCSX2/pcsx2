#include <stddef.h>

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>


uint32_t cpuinfo_compute_max_cache_size(const struct cpuinfo_processor* processor) {
  if (processor->cache.l4 != NULL) {
    return processor->cache.l4->size;
  } else if (processor->cache.l3 != NULL) {
    return processor->cache.l3->size;
  } else if (processor->cache.l2 != NULL) {
    return processor->cache.l2->size;
  } else if (processor->cache.l1d != NULL) {
    return processor->cache.l1d->size;
  }
  return 0;
}
