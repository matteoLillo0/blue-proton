#pragma once

// Mini framework di test: CHECK conta i fallimenti invece di fermarsi al primo,
// cosi' un solo lancio mostra tutto quello che non va.

#include <iostream>

namespace bp_test {
inline int g_failures = 0;
} // namespace bp_test

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << __FILE__ << ':' << __LINE__ << ": FALLITO: " #cond << '\n'; \
            ++bp_test::g_failures;                                                    \
        }                                                                             \
    } while (false)

void run_tracker_tests();
void run_yolo_postprocess_tests();
