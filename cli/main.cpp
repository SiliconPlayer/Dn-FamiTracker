/*
 * dnfamitracker-cli - Command-line test harness for Dn-FamiTracker
 */

#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "dnfamitracker-cli (scaffold)" << std::endl;
    if (argc > 1) {
        std::cout << "Target file: " << argv[1] << std::endl;
    }
    return 0;
}
