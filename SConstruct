# Build System for as
import os
import sys
studio=os.path.abspath('./com/as.tool/config.infrastructure.system/')
sys.path.append(studio)
from building import *

asenv = PrepareEnv()
BDIR = asenv['BDIR']
target = asenv['target']

objs = SConscript('SConscript',variant_dir=BDIR, duplicate=0)

Building(target,objs)
