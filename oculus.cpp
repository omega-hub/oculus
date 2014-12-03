#include <omega.h>
#include "OculusDisplaySystem.h"
#include "omega/PythonInterpreterWrapper.h"

BOOST_PYTHON_MODULE(oculus)
{
	SystemManager::instance()->setDisplaySystem(new OculusDisplaySystem());
}
