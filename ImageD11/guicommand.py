

# ImageD11_v0.4 Software for beamline ID11
# Copyright (C) 2005  Jon Wright
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
Interface between Tkinter gui and the actual useful code.

There should be no scientific algorithms (eventually) on
the gui side of this class.

This class will eventually offer macro recording capability.
"""


# Things to offer from gui
from ImageD11 import peakmerge, indexing, transform


class guicommand:
   """
   Keeps a log of all commands issued - separates gui code from
   algorithmical code
   """
   def __init__(self):
      self.objects = { "peakmerger" : peakmerge.peakmerger(), 
                       "transformer": None,
                       "indexer"    : indexing.indexer()
                       }
                       
      self.commandscript = """
from ImageD11 import peakmerger, indexing, transform
mypeakmerger = peakmerger.peakmerger()
mytransformer = transform.transformer()
myindexer = indexing.indexer()      
"""

   def execute(self,object,command,*args,**kwds):
      """
      Pass in object as string [peakmerger|transformer|indexer]
      Pass in command as string, getattr(command) will be used
      Returns the return value of the function....

      TODO : change this interface???
           eg : works - returns True
                        you look for self.lastreturned
                fails - returns False 
                        you look for self.lasttraceback
      """
      if object not in self.objects.keys():
         raise Exception("ERROR! Unknown command object")
      o = self.objects[object]
      func = getattr(o,command) 
      try:
         ran = "my%s.%s("%(object,command)
         addedcomma = ""
         for a in args:
            ran="%s %s %s"%(ran,addedcomma,repr(a))
            addedcomma=","
         for k,v in kwds.items():
            ran="%s %s %s=%s "%(ran,addedcomma,k,v)
            addedcomma=","
         ran+=" )"
         ret = func(*args, **kwds)  
      except:
         print self
         print object
         print command
         print func
         print args
         print kwds
         raise
      print "Ran:",ran
      self.commandscript+=ran
      return ret
 
   def getdata(self,object,name):
      """
      Allows access to "live" data in the objects wrapped
      
      By passing references back you can circumvent the 
      cleanliness of the interface. Please dont.

      Returns object.name 
      """
      if object not in self.objects.keys():
         raise Exception("ERROR! Unknown command object")
      attribute = getattr(self.objects[object],name)
      return attribute

