/*
  (C) Nguyen Ba Ngoc 2021
*/

#include "gsl.h"

gsl_t gsl_create(gtype_free_t free_value) {
  gsl_t list = malloc(sizeof(struct gsl_s));
  list->free_value = free_value;
  list->front = list->back = NULL;
  return list;
}

long gsl_size(gsl_t list) {
  gtype *tmp = gsl_front(list);
  long sz = 0;
  while (tmp) {
    ++sz;
    tmp = gsn_next(tmp);
  }
  return sz;
}