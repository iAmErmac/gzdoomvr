#include "gl/scene/rift_initializer.h"
#include "gl/scene/gl_oculustracker.h"

void initialize_oculus_rift() {
    sharedOculusTracker->checkInitialized();
}

