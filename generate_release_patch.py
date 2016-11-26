#!/usr/bin/env python

import os, sys
import filecmp
from distutils import dir_util
import shutil
holderlist=[]

def compareme(dir1, dir2):
    dircomp=filecmp.dircmp(dir1,dir2)
    only_in_one=dircomp.left_only
    diff_in_one=dircomp.diff_files
    dirpath=os.path.abspath(dir1)
    [holderlist.append(os.path.abspath( os.path.join(dir1,x) )) for x in only_in_one]
    [holderlist.append(os.path.abspath( os.path.join(dir1,x) )) for x in diff_in_one]
    if len(dircomp.common_dirs) > 0:
        for item in dircomp.common_dirs:
            compareme(os.path.abspath(os.path.join(dir1,item)), os.path.abspath(os.path.join(dir2,item)))
        return holderlist
    
dir1 = "./redist/"
dir2 = "C:/Program Files (x86)/Steam/steamapps/common/Penumbra Overture/redist/"
dir3 = "./release/"

if not dir3.endswith('/'): dir3=dir3+'/'

source_files=compareme(dir1,dir2)

dir1=os.path.abspath(dir1)
dir3=os.path.abspath(dir3)

print source_files[0]
print dir1
print dir3
print source_files[0].replace(dir1, dir3)

destination_files=[]
new_dirs_create=[]
for item in source_files:
    destination_files.append(item.replace(dir1, dir3) )
for item in destination_files:
    new_dirs_create.append(os.path.split(item)[0])
for mydir in set(new_dirs_create):
    if not os.path.exists(mydir): os.makedirs(mydir)

copy_pair=zip(source_files,destination_files)
for item in copy_pair:
    if os.path.isfile(item[0]):
        print "copying", item[0], '->', item[1]
        shutil.copyfile(item[0], item[1])
