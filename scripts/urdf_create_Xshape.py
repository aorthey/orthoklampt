import os 
import numpy as np
from math import cos,sin,pi,atan2
from urdf_create import *
from urdf_create_primitives import *

L1 = 1.4
thicknessx = 0.1
thicknessy = L1
spacing = 0.001

robot_name = 'Xshape'
folder='Lshape/'
fname = getPathname(folder, robot_name)

f = open(fname,'w')
f.write('<?xml version="1.0"?>\n')
f.write('<robot name="'+robot_name+'">\n')

hstr  = createCuboid("link0", 0,                            0, 0,                         thicknessx, thicknessy, thicknessx)
hstr += createCuboid("link1", -L1/2-thicknessx/2-spacing,   0, 0,                         L1,         thicknessy, thicknessx)
hstr += createCuboid("link2", 0,                            0, L1/2+thicknessx/2+spacing, thicknessx, thicknessy, L1)
hstr += createCuboid("link3", 0,                            0, -L1/2-thicknessx/2-spacing,thicknessx, thicknessy, L1)
hstr += createCuboid("link4", L1/2+thicknessx/2+spacing,    0, 0,                         L1,         thicknessy, thicknessx)

hstr += createRigidJoint("joint_"+"l0"+"_"+"l1", "link0", "link1")
hstr += createRigidJoint("joint_"+"l0"+"_"+"l2", "link0", "link2")
hstr += createRigidJoint("joint_"+"l0"+"_"+"l3", "link0", "link3")
hstr += createRigidJoint("joint_"+"l0"+"_"+"l4", "link0", "link4")

f.write(hstr)
f.write('  <klampt package_root="../../.." default_acc_max="4" >\n')
f.write('  </klampt>\n')
f.write('</robot>')
f.close()

print "\nCreated new file >>",fname

### create nested robots
CreateSphereRobot(folder, robot_name + "_sphere_inner", thicknessx/2)

CreateSphereRobot(folder, robot_name + "_sphere_outer", L1)

CreateCylinderRobot(folder, robot_name + "_capsule_inner", thicknessx/2, L1)

CreateCylinderRobot(folder, robot_name + "_capsule_outer", L1+thicknessx/2, L1)
