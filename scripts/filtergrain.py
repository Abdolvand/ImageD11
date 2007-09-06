## Automatically adapted for numpy.oldnumeric Sep 06, 2007 by alter_code1.py

#!/bliss/users/blissadm/python/bliss_python/suse82/bin/python

from ImageD11 import refinegrains
import numpy.oldnumeric as Numeric
import sys
fltfile = sys.argv[1] #"peaks.out_merge_t100"
parfile = sys.argv[2]
ubifile = sys.argv[3]
outfile = sys.argv[4]
notfile = sys.argv[5] # the stuff NOT indexed

o = refinegrains.refinegrains(tolerance=0.02)

o.loadparameters(parfile)
o.readubis(ubifile)
o.loadfiltered(fltfile)
o.generate_grains()
for tol in range(1,11):
    o.tolerance = tol*0.01
    print "tol",o.tolerance,
    o.refineubis(quiet=False)
o.tolerance = float(raw_input("Enter tolerance"))
for i,gn in zip(range(len(o.grainnames)),o.grainnames):
    print i,gn
gn = o.grainnames[int(raw_input("select which grain"))]
o.compute_gv(gn,fltfile)
matrix = o.refine(o.grains[(gn,fltfile)].ubi)

h=Numeric.matrixmultiply(matrix,Numeric.transpose(o.gv))
hint=Numeric.floor(h+0.5).astype(Numeric.Int) # rounds down
diff=h-hint
drlv=Numeric.sqrt(Numeric.sum(diff*diff,0))

print o.scandata[fltfile].shape
out = open(outfile,"H")
out.write("# xc yc omega npixels avg_intensity x_raw y_raw sigx sigy covxy\n")
out2 = open(notfile,"w")
out2.write("# xc yc omega npixels avg_intensity x_raw y_raw sigx sigy covxy\n")
for i in range(o.scandata[fltfile].shape[0]):
    if drlv[i] < o.tolerance:
        for v in o.scandata[fltfile][i,:]:
            out.write("%f "%(v))
        out.write("\n")
    else:
        for v in o.scandata[fltfile][i,:]:
            out2.write("%f "%(v))
        out2.write("\n")
out.close()
out2.close()

