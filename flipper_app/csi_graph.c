#include "csi_graph.h"
uint32_t csi_graph_scale(uint32_t threshold, uint32_t peak) {
    uint32_t t = threshold + threshold / 4, p = peak + peak / 10;
    uint32_t scale = t > p ? t : p;
    return scale < 60 ? 60 : scale;
}
int csi_graph_y(uint32_t value, uint32_t scale) {
    if(!scale) scale = 60;
    if(value > scale) value = scale;
    return 38 - (int)((uint64_t)value * 22 / scale);
}
