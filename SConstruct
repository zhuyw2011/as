# Build System for as
import os
import sys
studio=os.path.abspath('./com/as.tool/config.infrastructure.system/')
sys.path.append(studio)
from building import *

asenv = PrepareEnv()
ASROOT = asenv['ASROOT']
BDIR = asenv['BDIR']
target = asenv['target']

objs = SConscript('%s/SConscript'%(ASROOT),variant_dir=BDIR, duplicate=0)

Building(target,objs)
