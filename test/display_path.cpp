#include <stdio.h>
//#include <Interface/SimulationGUI.h>
#include <Interface/SimTestGUI.h>
#include <KrisLibrary/GLdraw/GL.h>
#include <KrisLibrary/GLdraw/drawextra.h>

#include <Contact/Grasp.h> //class Grasp
#include <Planning/ContactCSpace.h>
#include <Modeling/MultiPath.h>
#include <Planning/PlannerSettings.h>
#include <KrisLibrary/planning/AnyMotionPlanner.h>
#include <Modeling/DynamicPath.h>
#include <Modeling/Paths.h>
#include <Control/PathController.h>

#include "gui.h"
#include "info.h"
#include "environment_loader.h"

int main(int argc,const char** argv) {

  EnvironmentLoader env = EnvironmentLoader("data/snake_turbine.xml");
  env.GetBackendPtr()->Load("snake_turbine_complete.xml");

  GLUIForceFieldGUI gui(env.GetBackendPtr(),env.GetWorldPtr());
  gui.SetWindowTitle("SweptVolumePath");
  gui.Run();

  return 0;
}



