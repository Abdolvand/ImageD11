#!/usr/bin/python

# Separate this from the command line script.

## Automatically adapted for numpy.oldnumeric Sep 06, 2007 by alter_code1.py

#! /bliss/users/blissadm/python/bliss_python/suse82/bin/python


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



"""
Script for peaksearching images from the command line

Uses the connectedpixels extension for finding blobs above a threshold
and the blobcorrector(+splines) for correcting them for spatial distortion

Defines one function (peaksearch) which might be reused
"""

import time

# For benchmarking
reallystart = time.time()
global stop_now
stop_now = False

from math import sqrt
import sys , glob , os.path

from ImageD11 import blobcorrector
from ImageD11.labelimage import labelimage
import numpy

# Generic file format opener from fabio
from fabio.openimage import openimage
from fabio import file_series, fabioimage

# Global variables
OMEGA = 0
OMEGASTEP = 1.0
OMEGAOVERRIDE = False

class timer:
    def __init__(self):
        self.start = time.time()
        self.now = self.start
        self.msgs = []
    def msg(self,msg):
        self.msgs.append(msg)
    def tick(self,msg=""):
        now = time.time()
        self.msgs.append("%s %.2f/s"%(msg,now-self.now))
        self.now = now
    def tock(self,msg=""):
        self.tick(msg)
        print " ".join(self.msgs),"%.2f/s"% (self.now-self.start)
        sys.stdout.flush()

def read_and_correct(filename,
                     dark = None,
                     flood = None,
                     darkoffset = 0):
    """
    Reads a file doing the dark and flood corrections
    """
    try:
        data_object = openimage(filename)
    except KeyboardInterrupt:
        raise
    except:
        sys.stdout.write(filename+" not found\n")
        return 0
    picture = data_object.data.astype(numpy.float32)
    if dark != None:
        # This is meant to be quicker
        picture = numpy.subtract( picture , dark, picture )
        data_object.data = picture
    if flood != None:
        picture = numpy.divide( picture, flood, picture )
        data_object.data = picture
    if darkoffset != None and darkoffset != 0:
        picture = numpy.add( picture , darkoffset, picture )
        data_object.data = picture
    return data_object





def peaksearch( filename , 
                data_object , 
                corrector , 
                thresholds ,  
                labims ):
    """
    filename  : The name of the image file for progress info 	
    data_object : Fabio object containing data and header 
    corrector : spatial and dark/flood , linearity etc
    
    thresholds : [ float[1], float[2] etc]

    labims : label image objects, one for each threshold
    
    """
    t = timer()
    picture = data_object.data.astype(numpy.float32)

    for lio in labims.values():
        f = lio.sptfile
        f.write("\n\n# File %s\n" % (filename))
        f.write("# Processed on %s\n" % (time.asctime()))
        try:
            f.write("# Spatial correction from %s\n" % (corrector.splinefile))
            f.write("# SPLINE X-PIXEL-SIZE %s\n" % (str(corrector.xsize)))
            f.write("# SPLINE Y-PIXEL-SIZE %s\n" % (str(corrector.ysize)))
        except:
            pass
        for item in data_object.header.keys():
            if item == "headerstring": # skip
                continue
            try:
                f.write("# %s = %s\n" % (item,
                        str(data_object.header[item]).replace("\n"," ")))
            except KeyError:
                pass

    # Get the rotaiton angle for this image
    global OMEGA, OMEGASTEP, OMEGAOVERRIDE
    if not data_object.header.has_key("Omega") or OMEGAOVERRIDE:
        # Might have imagenumber or something??
        ome = OMEGA
        OMEGA += OMEGASTEP
        # print "Overriding the OMEGA"
    else: # Might have imagenumber or something??
        ome = float(data_object.header["Omega"])
    # print "Reading from header"
    #
    # Now peaksearch at each threshold level    
    t.tick(filename)
    for threshold in thresholds:
        labelim = labims[threshold]
        f = labelim.sptfile
        if labelim.shape != picture.shape:
            raise "Incompatible blobimage buffer for file %s" %(filename)
        #
        #
        # Do the peaksearch
        f.write("# Omega = %f\n"%(ome))
        labelim.peaksearch(picture, threshold, ome)
        f.write("# Threshold = %f\n"%(threshold))
        f.write("# npks = %d\n"%(labelim.npk))
        #
        if labelim.npk > 0:
            labelim.output2dpeaks(f)
        labelim.mergelast()
        t.msg("T=%-5d n=%-5d;" % (int(threshold),labelim.npk)) 
        # Close the output file
    # Finish progress indicator for this file
    t.tock()
    sys.stdout.flush()
    return None 


def peaksearch_driver(options, args):
    """
    To be called with options from command line
    """
    ################## debugging still
    for a in args:
        print "arg:"+str(a)
    for o in options.__dict__.keys():
        print "option:",str(o),str(getattr(options,o))
    ###################


    if options.thresholds is None:
        raise ValueError("No thresholds supplied [-t 1234]")

    if len(args) == 0  and options.stem is None:
        raise ValueError("No files to process")

        # What to do about spatial
 
    if options.perfect=="N" and os.path.exists(options.spline):
        print "Using spatial from",options.spline
        corrfunc = blobcorrector.correctorclass(options.spline)
    else:
        print "Avoiding spatial correction"
        corrfunc = blobcorrector.perfect()



    # Get list of filenames to process
    if len(args) > 0 :
        # We assume unlabelled arguments are filenames 
        file_series_object = file_series.file_series(args)
    else:
        if options.format in ['bruker', 'BRUKER', 'Bruker']:
            extn = ""
            corrfunc.orientation = "bruker"
        elif options.format == 'GE':
            extn = ""
        else:
            extn = options.format
            corrfunc.orientation = "edf"
        file_series_object = file_series.numbered_file_series(
                options.stem,
                options.first,
                options.last,
                extn,
                digits = options.ndigits,
                padding = options.padding )
    # Output files:

    if options.outfile[-4:] != ".spt":
        options.outfile = options.outfile + ".spt"
        print "Your output file must end with .spt, changing to ",options.outfile

    # Omega overrides

    global OMEGA, OMEGASTEP, OMEGAOVERRIDE
    OMEGA = options.OMEGA
    OMEGASTEP = options.OMEGASTEP
    OMEGAOVERRIDE = options.OMEGAOVERRIDE 
    # Make a blobimage the same size as the first image to process


    # List comprehension - convert remaining args to floats - must be unique list        
    thresholds_list = [float(t) for t in options.thresholds]
    import sets
    thresholds_list = list(sets.Set(thresholds_list))
    thresholds_list.sort()
        
    li_objs={} # label image objects, dict of
    s = openimage(file_series_object.current()).data.shape # data array shape
    # Create label images
    for t in thresholds_list:
        # the last 4 chars are guaranteed to be .spt above
        mergefile="%s_t%d.flt"%(options.outfile[:-4], t)
        spotfile = "%s_t%d.spt"%(options.outfile[:-4], t)
        li_objs[t]=labelimage(shape = s, 
                              fileout = mergefile, 
                              spatial = corrfunc,
                              sptfile=spotfile) 
        print "make labelimage",mergefile,spotfile
    # Not sure why that was there (I think if glob was used)
    # files.sort()
    if options.dark!=None:
        print "Using dark (background)",options.dark,"with added offset",options.darkoffset
        darkimage= openimage(options.dark).data 
    else:
        darkimage=None
    if options.flood!=None:
        floodimage=openimage(options.flood).data
        cen0 = floodimage.shape[0]/6
        cen1 = floodimage.shape[0]/6
        middle = floodimage[cen0:-cen0, cen1:-cen1]
        nmid = middle.shape[0]*middle.shape[1]
        floodavg = numpy.mean(middle)
        print "Using flood",options.flood,"average value",floodavg
        if floodavg < 0.7 or floodavg > 1.3:
            print "Your flood image does not seem to be normalised!!!"
         
    else:
        floodimage=None
    start = time.time()
    print "File being treated in -> out, elapsed time"
    # If we want to do read-ahead threading we fill up a Queue object with data 
    # objects
    # THERE MUST BE ONLY ONE peaksearching thread for 3D merging to work
    # there could be several read_and_correct threads, but they'll have to get the order right,
    # for now only one
    if options.oneThread:
        # Wrap in a function to allow profiling (perhaps? what about globals??)
        def go_for_it(file_series_object, darkimage, floodimage, 
                darkoffset, corrfunc , thresholds_list , li_objs ):
            for filein in file_series_object:
                t = timer()
                data_object = read_and_correct( filein, darkimage, floodimage,
                                                darkoffset)
                t.tick(filein+" io/cor")
                peaksearch( filein, data_object , corrfunc , 
                             thresholds_list , li_objs )
            for t in thresholds_list:
                li_objs[t].finalise()
        if options.profile_file is not None:
            print "Profiling output"
            try:
                import cProfile as Prof
            except ImportError:
                try:
                    import profile as Prof
                except:
                    print "Your package manager is having a laugh"
                    print "install python-profile please"
                    raise
            doff = options.darkoffset
            Prof.runctx( "go_for_it(file_series_object, darkimage, floodimage, \
                doff, corrfunc , thresholds_list, li_objs )",
                globals(),
                locals(),
                options.profile_file )
            import pstats
            try:
                p = pstats.Stats(options.profile_file,
                            stream = open(options.profile_file+".txt","w"))
            except:
                p = pstats.Stats(options.profile_file)
            p.strip_dirs().sort_stats(-1).print_stats()

        else:
            go_for_it(file_series_object, darkimage, floodimage, 
                options.darkoffset, corrfunc , thresholds_list, li_objs )
    else:
        print "Going to use threaded version!?!"
        try:
            import Queue, threading
            class read_all(threading.Thread):
                def __init__(self, queues, file_series_obj, dark , flood, darkoffset,
                        thresholds_list):
                    self.queues = queues 
                    self.file_series_obj = file_series_obj
                    self.dark = dark
                    self.flood = flood
                    self.darkoffset = darkoffset
                    self.thresholds_list = thresholds_list
                    threading.Thread.__init__(self)
                def run(self):
                    global stop_now
                    try:
                        for filein in self.file_series_obj:
                            if stop_now:
                                print "read_all is stopping"
                                break
                            ti = timer()
                            data_object = read_and_correct( filein, 
                                                            self.dark, self.flood,
                                                            self.darkoffset)
                            ti.tick(filein)
                            for t in self.thresholds_list:
                                # Hope that data object is read only
                                self.queues[t].put((filein, data_object) , block = True)
                            ti.tock(" enqueue ")
                        for t in self.thresholds_list:
                            self.queues[t].put( (None, None) , block = True)
                    except:
                        print "Exception in read_all thread"
                        stop_now = True
                        raise

            class peaksearch_one(threading.Thread):
                def __init__(self, q, corrfunc, threshold, li_obj ):
                    """ This will handle a single threshold and labelimage object """
                    self.q = q
                    self.corrfunc = corrfunc
                    self.threshold = threshold
                    self.li_obj = li_obj
                    threading.Thread.__init__(self)

                def run(self):
                    global stop_now
                    try:
                        while 1:
                            if stop_now:
                                print "Peaksearch is stopping"
                                break
                            filein, data_object = self.q.get(block = True)
                            if data_object is None:
                                break
                            peaksearch( filein, data_object , self.corrfunc , 
                                        [self.threshold] , 
                                        { self.threshold : self.li_obj } )  
                        self.li_obj.finalise()
                    except:
                        print "Exception in peaksearch_one"
                        stop_now = True
                        raise
            queues = {}
            searchers = {}
            for t in thresholds_list:
                "Print make queue and peaksearch for threshold",t
                queues[t] = Queue.Queue(5)
                searchers[t] = peaksearch_one(queues[t], corrfunc, 
                                              t, li_objs[t] )
            reader = read_all(queues, file_series_object, darkimage , floodimage, 
                    options.darkoffset, thresholds_list )
            reader.start()
            my_threads = [reader]
            for t in thresholds_list[::-1]:
                searchers[t].start()
                my_threads.append(searchers[t])
            looping = True
            nalive = len(my_threads)
            while nalive > 0:
                global stop_now
                try:
                    nalive = 0
                    for thr in my_threads:
                        reader.join(timeout=1)
                        if thr.isAlive():
                            nalive += 1
                    time.sleep(0.1)
                except KeyboardInterrupt:
                    print "Got keyboard interrupt in waiting loop"
                    stop_now = True
                    time.sleep(1)
                    for t in thresholds_list:
                        q = queues[t]
                        for i in range(10):
                            # empty out the queues
                            try:
                                q.get(block=False, timeout=1)
                            except:
                                break
                    print "finishing from waiting loop"
                except:
                    print "Caught exception in waiting loop"
                    stop_now = True
                    time.sleep(1)
                    for t in thresholds_list:
                        q = queues[t]
                        for i in range(10):
                            # empty out the queues
                            try:
                                q.get(block=False, timeout=0.1)
                            except:
                                break
                    for thr in my_threads:
                        if thr.isAlive():
                            thr.join(timeout=1)
                    raise
                    

        except ImportError:
            print "Probably no threading module present"
            raise
    


def get_options(parser):
        """ Add our options to a parser object """
        parser.add_option("-n", "--namestem", action="store",
            dest="stem", type="string", default="data",
            help="Name of the files up the digits part  "+\
                 "eg mydata in mydata0000.edf" )
        parser.add_option("-F", "--format", action="store",
            dest="format",default=".edf", type="string",
            help="Image File format, eg edf or bruker" )
        parser.add_option("-f", "--first", action="store",
            dest="first", default=0, type="int",
            help="Number of first file to process, default=0")
        parser.add_option("-l", "--last", action="store",
            dest="last", type="int",default =0,
            help="Number of last file to process")
        parser.add_option("-o", "--outfile", action="store",
            dest="outfile",default="peaks.spt", type="string",
            help="Output filename, default=peaks.spt")
        parser.add_option("-d", "--darkfile", action="store",
            dest="dark", default=None,  type="string",
            help="Dark current filename, to be subtracted, default=None")
        parser.add_option("-D", "--darkfileoffset", action="store",
            dest="darkoffset", default=10, type="int",
            help="Constant to subtract from dark to avoid overflows, default=100")
        s="/data/opid11/inhouse/Frelon2K/spatial2k.spline"
        parser.add_option("-s", "--splinefile", action="store",
            dest="spline", default=s, type="string",
            help="Spline file for spatial distortion, default=%s" % (s))
        parser.add_option("-p", "--perfect_images", action="store",
               type="choice", choices=["Y","N"], default="N", dest="perfect",
                          help="Ignore spline Y|N, default=N")
        parser.add_option("-O", "--flood", action="store", type="string",
                          default=None, dest="flood",
                          help="Flood file, default=None")
        parser.add_option("-t", "--threshold", action="append", type="float",
             dest="thresholds", default=None,
             help="Threshold level, you can have several")
        parser.add_option("--OmegaFromHeader", action="store_false",
                          dest="OMEGAOVERRIDE", default=False, 
                          help="Read Omega values from headers [default]")
        parser.add_option("--OmegaOverRide", action="store_true",
                          dest="OMEGAOVERRIDE", default=False, 
                          help="Override Omega values from headers")
        parser.add_option("--singleThread", action="store_true",
                          dest="oneThread", default=False, 
                          help="Do single threaded processing")
        parser.add_option("--profile", action="store", type="string",
                          dest="profile_file", default=None, 
                          help="Write profiling information (you will want singleThread too)")
        parser.add_option("-S","--step", action="store",
                          dest="OMEGASTEP", default=1.0, type="float",
                          help="Step size in Omega when you have no header info")
        parser.add_option("-T","--start", action="store",
                          dest="OMEGA", default=0.0, type="float",
                          help="Start position in Omega when you have no header info")
        parser.add_option("--ndigits", action="store", type="int",
                dest = "ndigits", default = 4,
                help = "Number of digits in file numbering [4]")
        parser.add_option("-P", "--padding", action="store",
               type="choice", choices=["Y","N"], default="Y", dest="padding",
                          help="Is the image number to padded Y|N, e.g. "\
                    "should 1 be 0001 or just 1 in image name, default=Y")
        return parser

if __name__=="__main__":
    raise Exception("Please use the driver script peaksearch.py")


