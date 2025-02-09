#include "simulator.hpp"

int main() {
    srand(static_cast <unsigned> (time(0)));
    Simulator sim = Simulator();
    return sim.run();
}
