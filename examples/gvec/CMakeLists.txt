add_executable(gvec_demo gvec_demo.c)
add_executable(gvec_i_demo gvec_i_demo.c)
add_executable(gvec_s_demo gvec_s_demo.c)
add_executable(gvec_pp_demo gvec_pp_demo.c)

foreach (p gvec_demo gvec_i_demo gvec_s_demo gvec_pp_demo)
  target_link_libraries(${p} bkc)
endforeach()