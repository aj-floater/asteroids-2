#include "app.h"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    try {
        App app;
        app.run();
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << "Asteroids failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
}
