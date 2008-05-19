



# ImageD11_v1.0 Software for beamline ID11
# Copyright (C) 2005-2007  Jon Wright
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

"""
Setup script for python distutils package

Can be used to generate source and binary distributions

On windows (with mingw tools installed, from www.mingw.org:
    python setup.py build --compiler=mingw32 sdist bdist bdist_wininst

In Linux
    python setup.py build sdist

Once assumed you have the "g77" compiler available for fortran and that
the usual (gnu linux/win32 x86) name mangling are done for the splines
extension to work ... now that code went through f2c, so it should just
work ??

"""




from distutils.core import setup, Extension


from numpy.distutils.misc_util import get_numpy_include_dirs

nid = get_numpy_include_dirs()


# Compiled extensions:

# closest is for indexing grains
cl = Extension("closest", 
               sources=['src/closest.c'], 
               include_dirs = nid )

# connectedpixels is for peaksearching images



cp = Extension("connectedpixels", 
               sources = ['src/connectedpixels.c','src/blobs.c'], 
               include_dirs = nid)
# No header files for distutils as sources 'src/dset.h'])


# histogramming thing
ch = Extension("_hist", 
               sources = ['src/hist.c'],
               include_dirs = nid )


# _splines is for correcting peak positions for spatial distortion
bl = Extension("_splines", 
               sources = ['src/splines.c', 'src/bispev.c'],
               include_dirs = nid)


# See the distutils docs...
setup(name='ImageD11', 
      version='1.1.1_svn',
      author='Jon Wright',
      author_email='wright@esrf.fr',
      description='ImageD11',
      license = "GPL",
      ext_package = "ImageD11",   # Puts extensions in the ImageD11 directory
      ext_modules = [cl,cp,bl,ch],
      packages = ["ImageD11"],
      package_dir = {"ImageD11":"ImageD11"},
      package_data = {"ImageD11" : ["doc/*.html"]},
      scripts = ["scripts/peaksearch.py",
                 "scripts/fitgrain.py",
                 "scripts/ubi2cellpars.py",
                 "scripts/filtergrain.py",
                 "scripts/filterout.py",
                 "scripts/pars_2_sweeper.py",
                 "scripts/ImageD11_2_shelx.py",
                 "scripts/fit2dcake.py",
                 "scripts/edfheader.py",
                 "scripts/recoveromega.py",
                 "ImageD11/plot3d.py",
                 "scripts/id11_summarize.py",
                 "scripts/ImageD11_gui.py",
                 "scripts/bgmaker.py",
                 "scripts/makemap.py",
                 "scripts/plotedf.py",
                 "scripts/plotgrainhist.py",
                 "scripts/rubber.py",
                 "scripts/edf2bruker.py",
                 "scripts/index_unknown.py",
                 "scripts/ImageD11Server.py",
                 "scripts/powderimagetopeaks.py"])
