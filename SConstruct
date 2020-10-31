# Build System for as
import os
import sys
studio=os.path.abspath('./com/as.tool/config.infrastructure.system/')
sys.path.append(studio)
from building import *

asenv = PrepareEnv()
BOARD = asenv['BOARD']
ASROOT = asenv['ASROOT']

bdir = 'build/%s'%(BOARD)
if(BOARD == 'any'):
    ANY = os.getenv('ANY')
    bdir = '%s/%s'%(bdir, ANY)
    TARGET = ANY
else:
    TARGET = BOARD

asenv['BDIR'] = os.path.abspath(bdir)

objs = SConscript('%s/com/SConscript'%(ASROOT),variant_dir=bdir, duplicate=0)

Building(TARGET,objs)
