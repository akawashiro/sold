import sys
import os 

build_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'build')
sys.path.append(build_path)

import pybind_test

print(pybind_test.add(1,2))
