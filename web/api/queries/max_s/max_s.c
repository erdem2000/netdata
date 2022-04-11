// SPDX-License-Identifier: GPL-3.0-or-later

#include "max_s.h"

// ----------------------------------------------------------------------------
// max_s

struct stats_max {
    calculated_number max;
    size_t count;
};

void *stats_create_max(RRDR *r) {
    (void)r;
    return callocz(1, sizeof(struct stats_max));
}

// resets when switches dimensions
// so, clear everything to restart
void stats_reset_max(RRDR *r) {
    struct stats_max *g = (struct stats_max *)r->internal.grouping_data;
    g->max = 0;
    g->count = 0;
}

void stats_free_max(RRDR *r) {
    freez(r->internal.grouping_data);
    r->internal.grouping_data = NULL;
}

void stats_add_max(RRDR *r, calculated_number value) {
    if(!isnan(value)) {
        struct stats_max *g = (struct stats_max *)r->internal.grouping_data;

        if(!g->count || calculated_number_fabs(value) > calculated_number_fabs(g->max)) {
            g->max = value;
            g->count++;
        }
    }
}

calculated_number stats_flush_max(RRDR *r, RRDR_VALUE_FLAGS *rrdr_value_options_ptr) {
    struct stats_max *g = (struct stats_max *)r->internal.grouping_data;

    calculated_number value;

    if(unlikely(!g->count)) {
        value = 0.0;
        *rrdr_value_options_ptr |= RRDR_VALUE_EMPTY;
    }
    else {
        value = g->max;
    }

    g->max = 0.0;
    g->count = 0;

    return value;
}

