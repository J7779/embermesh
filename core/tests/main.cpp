// Single doctest runner for every core module. Test files live in core/tests/<module>/*.cpp and are
// picked up by glob. Wrap each file's cases in TEST_SUITE("<module>") so a module can be run alone:
//     ./ember_tests -ts=<module>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
