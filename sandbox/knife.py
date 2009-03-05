
import numpy as np

def cylinder_thickness( x, radius = 1.0 ):
    """
    Computes the thickness of a solid cylinder at a distance
    x from the surface
    
       /|     hypotenuse = h = r
      / |)    lower horizontal = l =(r - x)
     /  | )   vertical = v = thickness / 2
    ____|x|   h*h = l*l + v*v

    """
    # Set values outside of the cylinder to be at x == 0
    xt = np.where( (x < 0.0) | (x > 2.0*radius) ,
                   0.0,
                   x )
    return 2.0 * np.sqrt( xt * ( 2.0 * radius - xt) )
    

assert abs(cylinder_thickness( 0., 1. )      ) < 1e-9
assert abs(cylinder_thickness( 1., 1. ) - 2. ) < 1e-9


def cylinder_absorbtion( x, radius = 1.0, mu = 5.0 ):
    """
    Computes the x-ray absorbtion of a cylinder with given 'radius'
    at positions 'x' given absorbtion coefficient mu.
    Applies exp( -mu*t) to the thickness computed by cylinder_thickness
    """
    thickness = cylinder_thickness( x, radius )
    return np.exp( - 1.0 * mu * thickness )

# If mu == 1.0 and x == r then absorbtion = exp(-2)
assert abs( cylinder_absorbtion( 1, radius = 1.0, mu = 1.0) - 
            np.exp( -2.0 ) ) < 1e-9
               

FWHM_TO_SIGMA = 0.5/np.sqrt(2.0*np.log(2.0))
assert abs(FWHM_TO_SIGMA - 1/2.35482) < 1e-6

def gaussian_peak( x, position, fwhm ):
    """
    returns a normalised gaussian peak centered at position with fwhm
    It is normalised such that the sum of the returned array is 1.0
    to be used for convolutions
    """
    sigma2 = FWHM_TO_SIGMA * fwhm * FWHM_TO_SIGMA * fwhm
    xmp = x - position
    tmp = 0.5 * xmp * xmp / sigma2
    peak = np.exp( -tmp )
    return peak / peak.sum()

# if fwhm is 2 implies point at x0+1 will be 0.5 * point at x0
def test_peak_function( pf ):
    x = np.arange(10)
    g = pf( x , 5, 2 )
    assert abs( g[4]/g[5] - 0.5 ) < 1e-9
    assert abs( g.sum() - 1.0 ) < 1e-9


test_peak_function( gaussian_peak )

def lorentzian_peak( x, position, fwhm):
    """
    returns a normalised gaussian peak centered at position with fwhm
    It is normalised such that the sum of the returned array is 1.0
    to be used for convolutions
    """
    xmp = x - position
    peak = 1.0 * fwhm / ( 1.0 * xmp * xmp + fwhm * fwhm / 4.0 )
    return peak / peak.sum() 

test_peak_function( lorentzian_peak )

def pseudo_voigt_peak( x, position, fwhm, eta = 0.5 ):
    """
    Computes a pseudo_voigt as eta*lorentzian_peak + (1-eta)*gaussian_peak
    """
    gauss = gaussian_peak( x, position, fwhm )
    lorentz = lorentzian_peak( x, position, fwhm )
    psv = 1.0 * eta * lorentz + ( 1.0 - eta ) * gauss
    return psv

test_peak_function( pseudo_voigt_peak )



def convoluted_cylinder_absorbtion( x, 
                                    radius,
                                    position, 
                                    mu,
                                    fwhm,
                                    eta ):
    """
    x = input positions to compute absorbtion
    position = where the surface of the cylinder is
    radius = of the cylinder
    mu = absorbtion coeffcient
    fwhm = the beam width
    eta = the mixing parameter of pseudo voigt
    """
    # the cylinder absorbtion function puts x at 0
    psv = pseudo_voigt_peak( x, position, fwhm, eta )
    cyl_abs = np.zeros( x.shape, np.float ) 
    middle = x.mean()
    for i in range(len(x)):
        x0 = x[i]
        np.add( cyl_abs,
                   psv[i] * cylinder_absorbtion( x - x0,
                                                 radius, mu),
                   cyl_abs )
    return cyl_abs


import ImageD11.simplex

def get_spec_data(scan_number,
                  thespecfile,
                  x_col = "Vscan z",
                  y_col = "pico3"):
    specdata = thespecfile.select("%d"%(scan_number))
    x = specdata.datacol( x_col )
    y = specdata.datacol( y_col )
    return x, y


def fitscan( xdata,
             ydata,
             radius = 1000, # microns
             fwhm_guess = 1.0,
             eta_guess = 0.5,
             mu_guess = 5e-3,
             plot = True,
             plot_title = "Your fit"):
    """
    
    """
    # guess the position is the halfway point
    ymin = ydata.min()
    ymax = ydata.max()
    halfway_up = 0.5 * (ymax + ymin)
    high_side = np.where( ydata > halfway_up,
                          1, 0)
    xhigh = (xdata * high_side).sum() / high_side.sum()
    low_side = np.where( ydata < halfway_up,
                         1, 0)
    xlow =  (xdata * low_side).sum() / low_side.sum()

    if xhigh < xlow: # Scan is descending
        position = 0.5*(xhigh + xlow)        
    else: # Scan is ascending
        position = 2.0*radius + 0.5*(xhigh + xlow)         
    
    scale = ydata.max()
    mu = mu_guess
    eta = eta_guess
    fwhm = fwhm_guess

    guess = [ position, fwhm, scale , eta]
    steps = [ 0.1, fwhm/10., scale/100, 0.01 ]

    X = xdata
    DATA = ydata

    def fitfunc( args ):
        position, fwhm, scale , eta = args
        #print mu
        mu = 0.0051
        cyl_abs = convoluted_cylinder_absorbtion( X, radius, position,
                                                  mu, fwhm, eta )
        obs_calc = DATA - 1.0 * cyl_abs * scale
        return np.dot( obs_calc, obs_calc )

    fitter = ImageD11.simplex.Simplex( 
        fitfunc,
        guess,
        steps )
    
    result, gof, nstep = fitter.minimize( maxiters = 1000,
                                          monitor =  1,
                                          epsilon = 0.001 )
    print "\nResults"
    names = "position fwhm scale eta"
    for name, value in zip(names.split(), result):
        print "%10s %f"%(name, value)
    mu = 0.0051
    if plot:
        from matplotlib.pylab import plot, show, cla, legend, subplot,\
            figure, title
        
        position, fwhm, scale, eta = result
        figure(1)
        subplot(211)
        cla()
        title(plot_title)
        plot( X, DATA, ".", label="obs")
        cyl_abs = convoluted_cylinder_absorbtion( X, radius, position,
                                                  mu, fwhm, eta )
        ycalc = 1.0 * cyl_abs * scale

        plot( X, ycalc, "-", label = "calc")
        legend()
        dyodx = (DATA[1:] - DATA[:-1])/(X[1:] - X[:-1])
        dx = 0.5*(X[1:] + X[:-1])
        subplot(212)
        cla()
        title("First Derivative (calc from fitted)")
        plot( dx, dyodx , ".", label="obs'")
        dycdx = (ycalc[1:] - ycalc[:-1])/(X[1:] - X[:-1])
        plot( dx, dycdx , "-", label="calc'")
        legend()
        show()

    return result


def main():
    import sys, specfile
    filename = sys.argv[1]
    myspecfile = specfile.Specfile( filename )
    scan_number = int(sys.argv[2])
    do_plot = False
    try:
        last = int(sys.argv[3])
    except:
        do_plot = True
        last = scan_number
        
    
        
    while scan_number <= last:
        x, y = get_spec_data(scan_number,
                         myspecfile )
    
        results = fitscan( x, y,
                           plot = do_plot,
                           plot_title = "%s #S %d"%(filename, scan_number))
        scan_number += 1

        f = open("knife_fits.dat", "a")
        f.write(("%s %d "+"%f "*len(results)+" \n")%tuple( [
                    filename, scan_number]+results ))
        f.close()
    #         radius = 1000, # microns
    #         fwhm_guess = 1.0,
    #         eta_guess = 0.5,
    #         mu_guess = 5e-3,
    #         plot = True 
    


if __name__=="__main__":
    main()
