# CMake generated Testfile for 
# Source directory: /srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/regression/bfr_evaluate
# Build directory: /srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/build-task92/regression/bfr_evaluate
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[bfr_evaluate_pos]=] "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/build-task92/bin/bfr_evaluate" "-all" "-silent" "-l" "3" "-pass" "2" "-d1")
set_tests_properties([=[bfr_evaluate_pos]=] PROPERTIES  _BACKTRACE_TRIPLES "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/regression/bfr_evaluate/CMakeLists.txt;31;add_test;/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/regression/bfr_evaluate/CMakeLists.txt;0;")
add_test([=[bfr_evaluate_uv1]=] "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/build-task92/bin/bfr_evaluate" "-all" "-silent" "-l" "3" "-pass" "0" "-skippos" "-uv" "-uvint" "1")
set_tests_properties([=[bfr_evaluate_uv1]=] PROPERTIES  _BACKTRACE_TRIPLES "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/regression/bfr_evaluate/CMakeLists.txt;33;add_test;/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/regression/bfr_evaluate/CMakeLists.txt;0;")
add_test([=[bfr_evaluate_uv5]=] "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/build-task92/bin/bfr_evaluate" "-all" "-silent" "-l" "3" "-pass" "0" "-skippos" "-uv" "-uvint" "5")
set_tests_properties([=[bfr_evaluate_uv5]=] PROPERTIES  _BACKTRACE_TRIPLES "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/regression/bfr_evaluate/CMakeLists.txt;35;add_test;/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/regression/bfr_evaluate/CMakeLists.txt;0;")
