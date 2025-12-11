#include "primitives.h"
#include "trainings.h"
#include "rwlock.h"

int main() {
    run_primitives_demo();
    run_trainings_demo();
    run_readers_writers_demo();
    return 0;
}
