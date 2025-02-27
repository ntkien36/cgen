#ifndef SPEC_D2W_H_
#define SPEC_D2W_H_

/*
  (C) Nguyen Ba Ngoc 2021
  Hàng đợi ưu tiên truy cập 2 chiều với kiểu double
*/

#include "p2w.h"

int d2w_push_with_index(p2wheap_t h, long idx, double elem);
double d2w_max(const p2wheap_t h);
double d2w_get(const p2wheap_t h, long idx);
double d2w_delete_max(p2wheap_t h);
double d2w_deactivate_max(p2wheap_t h);
double d2w_delete_max_index(p2wheap_t h, long *idx);
int d2w_modify(p2wheap_t h, long idx, double elem);

#endif  // SPEC_D2W_H_