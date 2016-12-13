#!/bin/bash

touch LOLOLOLOLOL
rm LOLOLOLOLOL `find . -name "*cc_o"`
touch LOLOLOLOLOL
rm LOLOLOLOLOL `find . -name "*.a"`
touch LOLOLOLOLOL
rm LOLOLOLOLOL `find . -name "*.a.fresh"`
touch LOLOLOLOLOL
rm LOLOLOLOLOL `find . -name "*cc_dep"`
touch LOLOLOLOLOL
rm LOLOLOLOLOL `find . -name "*.so"`
touch LOLOLOLOLOL
rm LOLOLOLOLOL `find . -name "*stats_o"`
touch LOLOLOLOLOL
rm LOLOLOLOLOL `find . -name "*iface_gcc"`

echo "All cleared"