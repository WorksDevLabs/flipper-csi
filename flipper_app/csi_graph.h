#pragma once
#include <stdint.h>
/* Linear visual scale only; motion thresholds/scores are never changed. */
uint32_t csi_graph_scale(uint32_t threshold, uint32_t peak);
int csi_graph_y(uint32_t value, uint32_t scale);
