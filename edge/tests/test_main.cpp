// Test senza framework esterni. Il processo esce con codice != 0 se un CHECK fallisce.
//   cmake --build edge/build && ctest --test-dir edge/build --output-on-failure

#include "check.hpp"

int main() {
    run_tracker_tests();
    run_yolo_postprocess_tests();

    if (bp_test::g_failures == 0) {
        std::cout << "Tutti i test passati.\n";
        return 0;
    }
    std::cerr << bp_test::g_failures << " controlli falliti.\n";
    return 1;
}
