

"""
Class to wrap the connectedpixels c extensions for labelling
blobs in images.
"""



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
# MERCHANTABILITY or FITESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  0211-1307  USA


from ImageD11 import blobcorrector, connectedpixels
# Names of property columns in array
from ImageD11.connectedpixels import s_1, s_I, s_I2,\
    s_fI, s_ffI, s_sI, s_ssI, s_sfI, s_oI, s_ooI, s_foI, s_soI, \
    bb_mn_f, bb_mn_s, bb_mx_f, bb_mx_s, bb_mn_o, bb_mx_o, \
    mx_I, mx_I_f, mx_I_s, mx_I_o
#    avg_i, f_raw, s_raw, o_raw, f_cen, s_cen, \
#    m_ss, m_ff, m_oo, m_sf, m_so, m_fo


from math import sqrt

import sys

import numpy.oldnumeric as n


# These should match the definitions in 
# /sware/exp/saxs/doc/SaxsKeywords.pdf
def flip1(x, y): 
    """ fast, slow to dety, detz"""
    return  x,  y
def flip2(x, y): 
    """ fast, slow to dety, detz"""
    return -x,  y   
def flip3(x, y): 
    """ fast, slow to dety, detz"""
    return  x, -y
def flip4(x, y): 
    """ fast, slow to dety, detz"""
    return -x, -y
def flip5(x, y): 
    """ fast, slow to dety, detz"""
    return  y,  x
def flip6(x, y): 
    """ fast, slow to dety, detz"""
    return  y, -x
def flip7(x, y): 
    """ fast, slow to dety, detz"""
    return -y,  x
def flip8(x, y): 
    """ fast, slow to dety, detz"""
    return -y, -x





class labelimage:
    """
    For labelling spots in diffraction images
    """

    titles = "#  fc  sc  omega" 
    format = "  %.4f"*3
    titles += "  Number_of_pixels"
    format += "  %.0f"
    titles += "  avg_intensity  f_raw  s_raw  sigf  sigs  covsf"
    format += "  %.4f"*6
    titles += "  sigo  covso  covfo"
    format += "  %.4f"*3
    titles += "  sum_intensity  sum_intensity^2"
    format += "  %.4f  %.4f"
    titles += "  IMax_int  IMax_f  IMax_s  IMax_o"
    format += "  %.4f  %.0f  %.0f  %.4f"
    titles += "  Min_f  Max_f  Min_s  Max_s  Min_o  Max_o"
    format += "  %.0f"*4 + "  %.4f"*2
    titles += "  dety  detz"
    format += "  %.4f"*2
    titles += "  onfirst  onlast"
    format += "  %d  %d"
    titles += "\n"
    format += "\n"


    def __init__(self,
                 shape, 
                 fileout = sys.stdout,
                 spatial = blobcorrector.perfect(),
                 flipper = flip2 ):
        """
        Shape - image dimensions
        fileout - writeable stream for merged peaks
        spatial - correction of of peak positions
        """
        self.shape = shape  # Array shape
        self.corrector = spatial  # applies spatial distortion
        self.fs2yz = flipper # generates y/z

        self.onfirst = 1    # Flag for first image in series
        self.onlast = 0     # Flag for last image in series
        self.blim = n.zeros(shape, n.Int)  # 'current' blob image 
        self.npk = 0        #  Number of peaks on current
        self.res = None     #  properties of current

        self.lastbl = n.zeros(shape, n.Int)# 'previous' blob image
        self.lastres = None
        self.lastnp = "FIRST" # Flags initial state

        self.verbose = 0    # For debugging


        if hasattr(fileout,"write"):
            self.outfile = fileout
        else:
            self.outfile = open(fileout,"w")

        self.spot3d_id = 0 # counter for printing

        self.outfile.write(self.titles)




    def peaksearch(self, data, threshold, omega):
        """
        # Call the c extensions to do the peaksearch, on entry:
        #
        # data = 2D Numeric array (of your data)
        # threshold = float - pixels above this number are put into objects
        """
        self.npk = connectedpixels.connectedpixels(data, 
                                                  self.blim, 
                                                  threshold,
                                                  self.verbose)
        if self.npk > 0:
            self.res = connectedpixels.blobproperties(data, 
                                                      self.blim, 
                                                      self.npk,
                                                      omega=omega)
        else:
            self.res = None

    def mergelast(self):
        """
        Merge the last two images searches
        """
        if self.lastnp == "FIRST":
            # No previous image available, this was the first
            # Swap the blob images
            self.lastbl, self.blim = self.blim, self.lastbl
            self.lastnp = self.npk
            self.lastres = self.res
            return
        ret = connectedpixels.bloboverlaps(self.lastbl,
                                           self.lastnp,
                                           self.lastres,
                                           self.blim,
                                           self.npk,
                                           self.res,
                                           self.verbose)

        # Fill out the moments of the "closed" peaks
        # print "calling blobmoments with",self.lastres
        ret = connectedpixels.blob_moments(self.lastres)
        # Write them to file
        self.outputpeaks(self.lastres)
        # lastres is now moved forward into res
        self.lastnp = self.npk   # This is array dim
        self.lastres = self.res  # free old lastres I hope
        # Also swap the blob images
        self.lastbl, self.blim = self.blim, self.lastbl

    def output2dpeaks(self):
        """
        Write something compatible with the old ImageD11 format
        which fabian is reading.
        This is called before mergelast, so we write self.npk/self.res
        """
        for i in self.res:
            if i[s_1] < 0.1:
                raise Exception("Empty peak on current frame")
            

        
    def outputpeaks(self, peaks):
        """
        Peaks are in Numeric arrays nowadays
        """

        for i in peaks:
            if i[s_1] < 0.1:
                # Merged with another
                continue
            avg_i = i[s_I]/i[s_1]
            # first moments
            fraw = i[s_fI]/i[s_I]
            sraw = i[s_sI]/i[s_I]
            oraw = i[s_oI]/i[s_I]
            # Spline correction
            fcen, scen = self.corrector.correct(fraw, sraw)
            # second moments - the plus 1 is the zero width = 1 pixel
            mss = sqrt( i[s_ssI]/i[s_I] - sraw*sraw + 1 ) 
            mff = sqrt( i[s_ffI]/i[s_I] - fraw*fraw + 1 ) 
            moo = sqrt( i[s_ooI]/i[s_I] - oraw*oraw + 1 ) 
            msf = ( i[s_sfI]/i[s_I] - sraw*fraw )/mss/mff   
            mso = ( i[s_soI]/i[s_I] - sraw*oraw )/mss/moo   
            mfo = ( i[s_foI]/i[s_I] - fraw*oraw )/mff/moo   

            dety, detz = self.fs2yz(fraw, sraw)
            self.outfile.write(self.format % (
                    fcen, scen, oraw, 
                    i[s_1], avg_i,    
                    fraw, sraw,
                    mss, mff, moo, msf, mso, mfo,
                    i[s_I],i[s_I2],  
                    i[mx_I],i[mx_I_f],i[mx_I_s],i[mx_I_o], 
                    i[bb_mn_f],i[bb_mx_f],i[bb_mn_s],i[bb_mx_s],
                    i[bb_mn_o],i[bb_mx_o],
                    dety, detz,
                    self.onfirst, self.onlast ))
            self.spot3d_id += 1
        if self.onfirst > 0:
            self.onfirst = 0
            
            
            
            
    def finalise(self):
        """
        Write out the last frame
        """
        self.onlast = 1
        ret = connectedpixels.blob_moments(self.lastres)
        self.outputpeaks(self.lastres)



