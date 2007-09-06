



/*
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

*/


static char moduledocs[] =\
"   C extensions for image analysis, part of ImageD11\n"\
"   ";



#include <Python.h>                  /* To talk to python */
/* #include "Numeric/arrayobject.h"      Access to Numeric */
#include "numpy/arrayobject.h"    /* Upgrade numpy */
#include "dset.h"   /* Disjoint sets thing for blob finding */
#include <time.h>  /* Benchmarking */
#include <unistd.h> /* ??? */
#include <stdlib.h> /* ??? */

/* used from From dset.h
int * dset_initialise(int size); 
int * dset_new(int ** S);
void dset_makeunion(int * S, int r1, int r2);
void dset_link(int *S, int r1, int r2);
int dset_find(int x, int * S);
*/

#undef PARANOID

double getval(char *p,int type);

double getval(char *p,int type){
   /* return the value of a python array element converted to the 
    * universal c type of double */

  switch (type){
     case    PyArray_CHAR   : return *(char          *)p*1.;
     case    PyArray_SHORT  : return *(short         *)p*1.;
     case    PyArray_INT    : return *(int           *)p*1.;
     case    PyArray_LONG   : return *(long          *)p*1.;
     case    PyArray_FLOAT  : return *(float         *)p*1.;
     case    PyArray_DOUBLE : return *(double        *)p*1.;
#ifdef PyArray_UNSIGNED_TYPES
     case    PyArray_UBYTE  : return *(unsigned char *)p*1.;
     case    PyArray_USHORT : return *(unsigned short*)p*1.;
     case    PyArray_UINT   : return *(unsigned int  *)p*1.;
#endif
     }
   printf("Oh bugger in getval - unrecongnised numeric type\n");
   exit(1);
   return 0;
}

void ptype(int type){
   /* Print out the type of a Numeric array from the C point of view */
  printf("Your input type was ");
  switch (type){
     case    PyArray_CHAR   : printf("PyArray_CHAR *(char *)\n");break;
     case    PyArray_SHORT  : printf("PyArray_SHORT *(short *)\n");break;
     case    PyArray_INT    : printf("PyArray_INT *(int  *)\n");break;
     case    PyArray_LONG   : printf("PyArray_LONG *(long *)\n");break;
     case    PyArray_FLOAT  : printf("PyArray_FLOAT *(float *)\n");break;
     case    PyArray_DOUBLE : printf("PyArray_DOUBLE *(double *)\n");break;
#ifdef PyArray_UNSIGNED_TYPES
     case    PyArray_UBYTE  : printf("PyArray_UBYTE *(unsigned char *)\n");break;
     case    PyArray_USHORT : printf("PyArray_USHORT *(unsigned short*)\n");break;
     case    PyArray_UINT   : printf("PyArray_UINT *(unsigned int *)\n");break;
#endif
     }
}


static char roisum_doc [] =\
"  (float) roisum ( Numeric.array(2D), xl , xh , yl , yh , verbose=0 ) \n"\
"   sum( array[xl:xh , yl:yh] ) where x,y refer to SLOW/FAST indices   \n"\
"   ... NOT Numeric indices, but slow/fast \n"\
"   Unsure why this was written - can be done with Numeric anyway     ";
   

/* Fill in an image of peak assignments for pixels */
static PyObject * roisum (PyObject *self, PyObject *args,  PyObject *keywds)
{
   PyArrayObject *dat=NULL; /* in (not modified) */
   int i,j,f,s,np;
   int xl,xh,yl,yh;
   int verbose=0,type;                /* whether to print stuff and the type of the input array */
   static char *kwlist[] = {"data","xl","xh","yl","yh","verbose", NULL};
   double sum;

   if(!PyArg_ParseTupleAndKeywords(args,keywds, "O!iiii|i",kwlist,      
				   &PyArray_Type, &dat,   /* array arg */
				   &xl,&xh,&yl,&yh,
				   &verbose))        /* optional verbosity */
     return NULL;

   if(verbose!=0){
     printf("\n\nHello there from roisum, you wanted the verbose output...\n");
      }

   /* Check array is two dimensional and Ushort */
   if(dat->nd != 2 ){    
     PyErr_SetString(PyExc_ValueError,
		     "Data array must be 2d,first arg problem");
     return NULL;
   }
   type=dat->descr->type_num;
   if(verbose!=0)ptype(type);

   /* Decide on fast/slow loop - inner versus outer */
   if(dat->strides[0] > dat->strides[1]) {
      f=1;  s=0;}
   else {
      f=0;  s=1;
   }
   if (verbose!=0){
        printf("Fast index is %d, slow index is %d, ",f,s);
        printf("strides[0]=%d, strides[1]=%d\n",dat->strides[0],dat->strides[1]);
   }

   /* Check ROI is sane */
   if(xl>xh || yl>yh || yl < 0 || xl < 0 || xh>dat->dimensions[s] || yh>dat->dimensions[f]){
     PyErr_SetString(PyExc_ValueError,
		     "Problem with your ROI");
     return NULL;
   }
   if(verbose!=0)printf("Summing ROI\n");
   sum=0.;
   np=0;
   for( i = xl ; i < xh ; i++ ){    /* i,j is looping along the indices data array */
     for( j = yl ; j < yh ; j++ ){
       sum+=getval((dat->data + i*dat->strides[s] + j*dat->strides[f]),type); 
       np++;
       if(verbose>2){
	 printf("%d %d %f\n",i,j,getval((dat->data + i*dat->strides[s] + j*dat->strides[f]),type)); 
       }
     }
   }
   if(verbose!=0){
	   printf("Sum %f np %d\n",sum,np);
   }
   return Py_BuildValue("d", sum/np); 
}

static char connectedpixels_doc[] =\
"   nblobs = connectedpixels ( data=Numeric.array(data, 2D)  , \n"\
"                              results=Numeric.array(blob, 2D, Int)  ,\n"\
"                              threshold=float threshold ,\n"\
"                              dark=Numeric.array(dark, 2D)  , \n"\
"                              flood=Numeric.array(flood, 2D)  , \n"\
"                              verbose=Int verbose )\n"\
"   data is normally an image \n"\
"   blob is an array to receive pixel -> blob assignments\n"\
"   threshold is the value above which a pixel is considered to be in a blob\n"\
"   verbose flags printing on stdout";

                              
                              /* Fill in an image of peak assignments for pixels */
static PyObject * connectedpixels (PyObject *self, PyObject *args,  PyObject *keywds)
{
   PyArrayObject *dataarray=NULL,*results=NULL; /* in (not modified) and out (modified) */
   PyArrayObject *dark=NULL,*flood=NULL; /* in (not modified)  */
   int i,j,k,l,f,s,np,ival;        
   int *T;                            /* for the disjoint set copy */
   int verbose=0,type,dktype=0,fltype=0;   /* whether to print stuff and the type of the input array */
   int percent,npover;                       /* for the progress indicator in verbose mode */
   static char *kwlist[] = {"data","results","threshold","dark","flood","verbose", NULL};
   clock_t tv1,tv1point5,tv2,tv3,tv4;
   double time,val,fl,dk;
   float threshold;
   if(!PyArg_ParseTupleAndKeywords(args,keywds, "O!O!f|O!O!i",kwlist,      
				   &PyArray_Type, &dataarray,   /* array args */
				   &PyArray_Type, &results,   /* array args */
				   &threshold,
				   &PyArray_Type, &dark,   /* array args */
				   &PyArray_Type, &flood,   /* array args */				   
				   &verbose))        /* threshold and optional verbosity */
      return NULL;
   if(verbose!=0){
      printf("\n\nHello there from connected pixels, you wanted the verbose output...\n");
      }

   tv1=clock();  
   /* Check array is two dimensional */
   if(dataarray->nd != 2){
     PyErr_SetString(PyExc_ValueError, "Array must be 2D!");
     return NULL;
   }
     /* Using int types now for Bruker images ...
	if(  dataarray->descr->type_num!=PyArray_USHORT){    
	PyErr_SetString(PyExc_ValueError,
	"Data array must be 2d, UInt16, first arg problem, easy fix in the C source if you really have something else");
	return NULL;
      }
     */
   type=dataarray->descr->type_num;
   if(verbose!=0)ptype(type);
   if(verbose!=0)printf("Thresholding at level %f\n",threshold);

   /*
    * Abandoned this - allocating in here gives memory leaks and is dumb
    * as you add an overhead which is not really needed here
    *
    * */
   /* make an array to hold the integer peak assignments */
   /*   PyArray_FromDims(int n_dimensions, int dimensions[n_dimensions], int type_num) */
   /* Make a new array to hold the results... how to do optional arguments?? */
   /*   results=(PyArrayObject*)PyArray_FromDims(dataarray->nd, dataarray->dimensions, PyArray_INT); */
   /*   if(results==NULL){*/
   /*   printf("Could not make a results array\nGiving up\n");*/
   /*   exit(2);   } */
   /*   if(verbose!=0)printf("Made results array\n");*/

   if(results->nd != 2){    
      PyErr_SetString(PyExc_ValueError,
                       "Array must be 2d, second arg problem");
      return NULL;
      }

   if( results->dimensions[0] != dataarray->dimensions[0] ||
       results->dimensions[1] != dataarray->dimensions[1] ){    
      PyErr_SetString(PyExc_ValueError,
                       "Arrays must have same shape");
      return NULL;
      }

   if(flood!=NULL){
     if(flood->nd != 2){    
      PyErr_SetString(PyExc_ValueError,
                      "Array must be 2d, flood problem");
      return NULL;
      }
     if( flood->dimensions[0] != dataarray->dimensions[0] ||
	 flood->dimensions[1] != dataarray->dimensions[1] ){    
       PyErr_SetString(PyExc_ValueError,
                       "Arrays must have same shape - flood doesnt match data");
       return NULL;
     }
     fltype=flood->descr->type_num;
     if(verbose!=0){printf("Using a flood image");}
   }
   if(dark!=NULL){
     if(dark->nd != 2){    
      PyErr_SetString(PyExc_ValueError,
                       "Array must be 2d, dark problem");
      return NULL;
      }
     if( dark->dimensions[0] != dataarray->dimensions[0] ||
	 dark->dimensions[1] != dataarray->dimensions[1] ){    
       PyErr_SetString(PyExc_ValueError,
                       "Arrays must have same shape - dark doesnt match data");
       return NULL;
     }
     dktype=dark->descr->type_num;
     if(verbose!=0){printf("Using a dark image");}
   }

   S = dset_initialise(16384); /* Default number before reallocation, rather large */
   if(verbose!=0)printf("Initialised the disjoint set\n");
   npover=0; /* number of pixels over threshold */
   /* Decide on fast/slow loop - inner versus outer */
   if(dataarray->strides[0] > dataarray->strides[1]) {
      f=1;  s=0;}
   else {
      f=0;  s=1;
   }
   if (verbose!=0){
        printf("Fast index is %d, slow index is %d, ",f,s);
        printf("strides[0]=%d, strides[1]=%d\n",dataarray->strides[0],dataarray->strides[1]);
   }
   percent=(results->dimensions[s]-1)/80.0;
   if(percent < 1)percent=1;
   tv1point5=clock();   
   if(verbose!=0){
     time=(tv1point5-tv1)/CLOCKS_PER_SEC;
     printf("Ready to scan image, setup took %g and clock is only good to ~0.015 seconds\n",time);
     }

   if(verbose!=0)printf("Scanning image\n");
   for( i = 0 ; i < (results->dimensions[s]) ; i++ ){    /* i,j is looping along the indices data array */

      if(verbose!=0 && (i%percent == 0) )printf(".");


      for( j = 0 ; j < (results->dimensions[f]) ; j++ ){

      	/* Set result for this pixel to zero - Not needed here? */
	
      	(*(int *)(results->data + i*results->strides[s] + j*results->strides[f])) = 0;


	/* CHANGE COMMENT HERE FOR UINT16 NUMERICAL TYPE, AND MAYBE MAKE IVAL A USHORT */
	/* ival= (* (unsigned short *) (dataarray->data + i*dataarray->strides[s] + j*dataarray->strides[f])) ;*/

	val=getval((dataarray->data + i*dataarray->strides[s] + j*dataarray->strides[f]),type); 

	if(dark!=NULL){
	  dk=getval((dark->data + i*dark->strides[s] + j*dark->strides[f]),dktype); 
	  val=val-dk;
	}
	if(flood!=NULL){
	  fl=getval((flood->data + i*flood->strides[s] + j*flood->strides[f]),fltype); 
	  val=val/fl;
	}
	
	if( val > threshold) {
             npover++;
             k=0;l=0;
             /* peak needs to be assigned */
             /* i-1, j-1, assign same index */
             if(i!=0 && j!=0)
                if( ( k=*(int *)(results->data + (i-1)*results->strides[s] + (j-1)*results->strides[f] )) >0 ) {
                       (*(int *)(results->data + i*results->strides[s] + j*results->strides[f])) = k;
                       l=k; /* l holds the assignment of the current pixel */
                       }
             /* i-1,j */
             if(i!=0)
                if( ( k=*(int *)(results->data + (i-1)*results->strides[s] + j*results->strides[f] )) >0 ) {
                    if(l==0){
                       (*(int *)(results->data + i*results->strides[s] + j*results->strides[f])) = k;
                       l=k; /* l holds the assignment of the current pixel */
                       } else {
                       if(l!=k)dset_makeunion(S,k,l);
                    }
              }
              /* i-1, j+1 */
              if(i!=0 && j!=(results->dimensions[f]-1))  
                 if( ( k=*(int *)(results->data + (i-1)*results->strides[s] + (j+1)*results->strides[f] )) >0 ) {
                    if(l==0){
                        (*(int *)(results->data + i*results->strides[s] + j*results->strides[f])) = k;
                        l=k; /* l holds the assignment of the current pixel */
                        } else {
                        if(l!=k)dset_makeunion(S,k,l);
                    }
              }
              /* i, j-1 */
              if(j!=0)
                 if( ( k=*(int *)(results->data + i*results->strides[s] + (j-1)*results->strides[f] )) >0 ) {
                    if(l==0){
                        (*(int *)(results->data + i*results->strides[s] + j*results->strides[f])) = k;
                        l=k; /* l holds the assignment of the current pixel */
                        } else {
                        if(l!=k)dset_makeunion(S,k,l);
                    }
              }
              if(l==0){ /* pixel has no neighbours thus far */
                 S = dset_new(&S);
                 (*(int *)(results->data + i*results->strides[s] + j*results->strides[f])) = S[S[0]-1];
              } 
    } /* active pixel */
    else { /* inactive pixel  - zero in results */
                 (*(int *)(results->data + i*results->strides[s] + j*results->strides[f])) = 0;
    }
      } /* j */
   } /* i */
   tv2=clock();   
   if(verbose!=0){
     printf("\nFinished scanning image, now make unique list and sums\n");
     printf("Number of pixels over threshold was : %d\n",npover);
     time=(tv2-tv1)/CLOCKS_PER_SEC;
     printf("That took %f seconds and %f in total being aware that the clock is only good to ~0.015 seconds\n",1.*(tv2-tv1point5)/CLOCKS_PER_SEC,time*1.);
     }
   /* First, count independent peaks */

   /* count peaks */
   np=S[S[0]-1];
   /* Now make each T[i] contain the unique ascending integer for the set */ 
   T=NULL;
   if(verbose!=0)printf("Now I want to malloc T=%p at size np=%d and sizeof(int)=%d\n",T,np,sizeof(int));
   if( ( T=(int *) ( malloc( (np+3)*sizeof(int) ) ) ) == NULL){
      printf("Memory allocation error in connected pixels\n");
      return 0;
   }
   if(verbose!=0)printf("Now T should be allocated, it is at %p\n",T);
   np=0;
   for(i=1;i<S[S[0]-1]+1;i++){
     if(S[i]==i){
      np++;
      T[i]=np;
      }
      else{  /* check */
        j=dset_find(i,S);
        T[i]=T[j];
        if(j>=i && verbose){
            printf("Oh dear - there was a problem compressing the disjoint set, j=%d, i=%d \n",j,i);
        }
        if(S[j]!=j && verbose!=0){
            printf("Oh dear - the disjoint set is squiff,S[j]=%d,j=%d\n",S[j],j);
        }
      }
   }
   if(verbose!=0)printf("\n");
   tv3=clock();
   if(verbose!=0){
     printf("Compressing list, time=%g, total time=%g\n",1.*(tv3-tv2)/CLOCKS_PER_SEC,1.*(tv3-tv1)/CLOCKS_PER_SEC);
     }

   if(verbose)printf("Found %d peaks\nNow assigning unique identifiers\n",np);


   /* Now loop through the results image again, assigning the corrected numbers to each pixel */
   for( i = 0 ; i < (results->dimensions[s]) ; i++ ){    /* i,j is looping along the indices data array */
      for( j = 0 ; j < (results->dimensions[f]) ; j++ ){
         ival=*(int *)(results->data + i*results->strides[s] + j*results->strides[f]);
         if(  ival > 0){
      if(T[ival]==0){
         printf("Something buggered up, ival=%d, T[ival]=%d, dset_find(ival,S)=%d\n",ival,T[ival],dset_find(ival,S));
         /* free(S); */
      exit(2);
      return NULL;
      }
      (*(int *)(results->data + i*results->strides[s] + j*results->strides[f]))=T[ival];
         }
      }
   }



 
   if(verbose!=0)printf("Freeing S=%p and T=%p\n",S,T);
   free(S);
   free(T);
   if(verbose!=0){
     printf("Freed S=%p and T=%p\n",S,T);
     printf("Finished assigning unique values and tidied up, now returning");
     tv4=clock();
     time=(tv4-tv3)/CLOCKS_PER_SEC;
     printf("\nThat took %g seconds, being aware that the clock is only good to ~0.015 seconds\n",1.*time);
     printf("The total time in this routine was about %f\n",1.*(tv4-tv1)/CLOCKS_PER_SEC); 
     }
   
/*   return PyArray_Return(results); */
/*   Py_DECREF(results);*/
     /*   Py_DECREF(dataarray);*/
   return Py_BuildValue("i", np+1);/* why the plus one?? */ 
}


/* BLOB PROPERTIES BIT - things to measure are: */
/* Centre of mass (x,y) - optionally use x,y image */
/* Amount of intensity in blob */
/* std dev of intensity in blob */
/* ...? */

static char blobproperties_doc[] =\
"   res = blobproperties ( Numeric.array(data, 2D)  , \n"\
"                          Numeric.array(blob, 2D, Int)  ,\n"\
"                          Int np , \n"\
"                          dark=Numeric.array(dark, 2D)  , \n"\
"                          flood=Numeric.array(flood, 2D)  , \n"\
"                          Int verbose )\n"\
"\n"\
"   Computes various properties of a blob image (created by connectedpixels)\n"\
"   data  = image data \n"\
"   blob  = integer peak assignments from connectedpixels \n"\
"   np    = number of peaks to treat \n"\
"   verbose  - flag about whether to print\n"\
"  \n"\
"   res = tuple (npix , sum , sumsq , com0 , com1 , com00 , com01 ,  com11 ) \n"\
"   ...where  fval = intensity in pixel and sum over pixels in blob of\n"\
"             anpix[peak]  =anpix[peak]  + 1       ;    // # of pixels\n"\
"             asum[peak]   =asum[peak]   +     fval;    // total intensity\n"\
"             asumsq[peak] =asumsq[peak] +fval*fval;    // total intensity^2\n"\
"             acom0[peak]  =acom0[peak]  +   i*fval;    // etc\n"\
"             acom1[peak]  =acom1[peak]  +   j*fval;\n"\
"             acom00[peak] =acom00[peak] + i*i*fval;\n"\
"             acom01[peak] =acom01[peak] + i*j*fval;\n"\
"             acom11[peak] =acom11[peak] + j*j*fval;";


   
static PyObject * blobproperties (PyObject *self, PyObject *args,  PyObject *keywds)
{
   PyArrayObject *dataarray=NULL,*blobarray=NULL; /* in (not modified) */
   PyArrayObject *dark=NULL,*flood=NULL; /* in (not modified) */
   static char *kwlist[] = {"data","blob","npeaks","dark","flood","verbose", NULL};
   PyArrayObject *sum,*sumsq,*com0,*com1,*npix, *com00, *com11, *com01;
   double *asum,*asumsq,*acom0,*acom1, *acom00, *acom11, *acom01 ;
   double fval,dk,fl;
   int np,verbose=0,type,f,s,peak,bad,fltype=0,dktype=0;
   int i,j,safelyneed,*anpix, percent;
   if(!PyArg_ParseTupleAndKeywords(args,keywds, "O!O!i|O!O!i",kwlist,      
                        &PyArray_Type, &dataarray,   /* array args - data */
                        &PyArray_Type, &blobarray,   /* blobs */
                        &np,              /* Number of peaks to treat */
                        &PyArray_Type, &dark,   /* array args */
                        &PyArray_Type, &flood,   /* array args */				   
                        &verbose))        /* threshold and optional verbosity */
      return NULL;

   /* Check array is two dimensional */
   if(dataarray->nd != 2){     
      PyErr_SetString(PyExc_ValueError,
                       "data array must be 2d, first arg problem");
      return NULL;
      }
   if(verbose!=0)printf("Welcome to blobproperties\n");
   /* Check array is two dimensional and int - results from connectedpixels above */
   if(blobarray->nd != 2 && blobarray->descr->type_num != PyArray_INT){     
      PyErr_SetString(PyExc_ValueError,
                       "Blob array must be 2d and integer, second arg problem");
      return NULL;
      }
   type=dataarray->descr->type_num;
   /* Decide on fast/slow loop - inner versus outer */
   if(blobarray->strides[0] > blobarray->strides[1]) {
      f=1;  s=0;}
   else {
      f=0;  s=1;
   }

   if(flood!=NULL){
     if(flood->nd != 2){    
      PyErr_SetString(PyExc_ValueError,
                      "Array must be 2d, flood problem");
      return NULL;
      }
     if( flood->dimensions[0] != dataarray->dimensions[0] ||
	 flood->dimensions[1] != dataarray->dimensions[1] ){    
       PyErr_SetString(PyExc_ValueError,
                       "Arrays must have same shape - flood doesnt match data");
       return NULL;
     }
     fltype=flood->descr->type_num;
     if(verbose!=0){printf("Using a flood image");}
   }
   if(dark!=NULL){
     if(dark->nd != 2){    
      PyErr_SetString(PyExc_ValueError,
                       "Array must be 2d, dark problem");
      return NULL;
      }
     if( dark->dimensions[0] != dataarray->dimensions[0] ||
	 dark->dimensions[1] != dataarray->dimensions[1] ){    
       PyErr_SetString(PyExc_ValueError,
                       "Arrays must have same shape - dark doesnt match data");
       return NULL;
     }
     dktype=dark->descr->type_num;
     if(verbose!=0){printf("Using a dark image");}
   }




   if (verbose!=0){
        printf("Fast index is %d, slow index is %d, ",f,s);
   printf("strides[0]=%d, strides[1]=%d\n",dataarray->strides[0],dataarray->strides[1]);
   }
  /* results arrays */
  safelyneed=np+3;
  npix  = (PyArrayObject *)PyArray_FromDims(1,&safelyneed,PyArray_INT);
  sumsq = (PyArrayObject *)PyArray_FromDims(1,&safelyneed,PyArray_DOUBLE);
  sum   = (PyArrayObject *)PyArray_FromDims(1,&safelyneed,PyArray_DOUBLE);
  com0  = (PyArrayObject *)PyArray_FromDims(1,&safelyneed,PyArray_DOUBLE);
  com1  = (PyArrayObject *)PyArray_FromDims(1,&safelyneed,PyArray_DOUBLE);
  com00 = (PyArrayObject *)PyArray_FromDims(1,&safelyneed,PyArray_DOUBLE);
  com01 = (PyArrayObject *)PyArray_FromDims(1,&safelyneed,PyArray_DOUBLE);
  com11 = (PyArrayObject *)PyArray_FromDims(1,&safelyneed,PyArray_DOUBLE);

  if (npix == NULL || sumsq == NULL || sum == NULL || com0 == NULL || com1 == NULL) goto fail;
  anpix  = (int   *)   npix->data;
  asumsq = (double *) sumsq->data;
  asum   = (double *)   sum->data;
  acom0  = (double *)  com0->data;
  acom1  = (double *)  com1->data;
  acom00 = (double *) com00->data;
  acom01 = (double *) com01->data;
  acom11 = (double *) com11->data;

  for ( i=0 ; i<safelyneed ; i++){
    anpix[i]  = 0;
    asumsq[i] = 0.;
    asum[i]   = 0.;
    acom0[i]  = 0.;
    acom1[i]  = 0.;
    acom00[i] = 0.;
    acom01[i] = 0.;
    acom11[i] = 0.;
  }   


   if(verbose!=0)printf("Got some arrays to put the results in\n");
   percent=(blobarray->dimensions[s]-1)/80.0;
   if(percent < 1)percent=1;
   bad=0;
   if(verbose!=0)printf("Scanning image\n");
   for( i = 0 ; i <= (blobarray->dimensions[s]-1) ; i++ ){    /* i,j is looping along the indices data array */
      if(verbose!=0 && (i%percent == 0) )printf(".");
      for( j = 0 ; j <= (blobarray->dimensions[f]-1) ; j++ ){
         peak=* (int *) (blobarray->data + i*blobarray->strides[s] + j*blobarray->strides[f]);
         if( peak > 0  && peak <=np ) {

	     fval=getval((dataarray->data + i*dataarray->strides[s] + j*dataarray->strides[f]),type); 

	     if(dark!=NULL){
	       dk=getval((dark->data + i*dark->strides[s] + j*dark->strides[f]),dktype); 
	       fval=fval-dk;
	     }
	     if(flood!=NULL){
	       fl=getval((flood->data + i*flood->strides[s] + j*flood->strides[f]),fltype); 
	       fval=fval/fl;
	     }
	
             asum[peak]   =asum[peak]   +     fval;
             asumsq[peak] =asumsq[peak] +fval*fval;
             acom0[peak]  =acom0[peak]  +   i*fval;
             acom1[peak]  =acom1[peak]  +   j*fval;
             acom00[peak] =acom00[peak] + i*i*fval;
             acom01[peak] =acom01[peak] + i*j*fval;
             acom11[peak] =acom11[peak] + j*j*fval;
             anpix[peak]  =anpix[peak]  + 1       ;
#ifdef PARANOID
             if(verbose!=0){
                printf("peak=%d i=%d j=%d val=%f acom00[peak]=%f 01=%f 11=%f\n",peak,i,j,fval,acom00[peak],
                        acom01[peak],acom11[peak]);}
#endif
    }
     else{
       if(peak!=0){
          bad++;
          if(verbose!=0 && bad<10){printf("Found %d in your blob image at i=%d, j=%d\n",peak,i,j);}
          /* Only complain 10 times - otherwise piles of crap go to screen */
          }
       }
      } /* j */
   } /* i */
   if(verbose){
     printf("\nFound %d bad pixels in the blob image\n",bad);
   }
   return Py_BuildValue("OOOOOOOO", PyArray_Return(npix),
                                    PyArray_Return(sum),
                                    PyArray_Return(sumsq),
                                    PyArray_Return(com0),
                                    PyArray_Return(com1),
                                    PyArray_Return(com00),
                                    PyArray_Return(com01),
                                    PyArray_Return(com11)   ); 
   fail:
    Py_XDECREF(npix);
    Py_XDECREF(sum);
    Py_XDECREF(sumsq);
    Py_XDECREF(com0);
    Py_XDECREF(com1);
    Py_XDECREF(com00);
    Py_XDECREF(com01);
    Py_XDECREF(com11);
    return NULL;
}


static char bloboverlaps_doc[] =\
"   res = bloboverlaps (   Numeric.array(blob1, 2D, Int), n1  , \n"\
"                          Numeric.array(blob2, 2D, Int), n2  , verbose=0) \n"\
" \n"\
" returns a disjoint set making the overlaps between blob1 and blob2 \n"\
" res[0->n1] = peaks in image 1 \n"\
" res[n2->n2+n1+1] = peaks in image 2 \n";
    

   
static PyObject * bloboverlaps (PyObject *self, PyObject *args,  PyObject *keywds)
{
   PyArrayObject *b1=NULL,*b2=NULL, *res=NULL; /* in (not modified) */
   int i,j,s,f, verbose ; /* loop vars, flag */
   int p1,p2,n1,n2; /* peak num and npeaks in each */
   int *link , percent, safelyneed;
   static char *kwlist[] = {"blob1","n1","blob2","n2","verbose", NULL};
   if(!PyArg_ParseTupleAndKeywords(args,keywds, "O!iO!i|i",kwlist,      
                        &PyArray_Type, &b1,   /* blobs */
                        &n1,
                        &PyArray_Type, &b2,   /* blobs */
                        &n2,
                        &verbose))        /* threshold and optional verbosity */
      return NULL;
   if(verbose!=0)printf("Welcome to bloboverlaps\n");
   
   /* Check array is two dimensional and int - results from connectedpixels above */
   
   if(b1->nd != 2 && b1->descr->type_num != PyArray_INT){     
      PyErr_SetString(PyExc_ValueError,
                       "Blob1 array must be 2d and integer, first arg problem");
      return NULL;
      }
   if(b2->nd != 2 && b2->descr->type_num != PyArray_INT){     
      PyErr_SetString(PyExc_ValueError,"Blob2 array must be 2d and integer, second arg problem");
      return NULL;
      }
   if (b1->strides[0]!=b2->strides[0] ||
       b1->strides[1]!=b2->strides[1] ||
       b1->dimensions[0]!=b2->dimensions[0] ||
       b1->dimensions[1]!=b2->dimensions[1] ){
      PyErr_SetString(PyExc_ValueError,
                       "Blob1 and Blob2 array be similar (dims & strides)");
      return NULL;
   }       
   /* Decide on fast/slow loop - inner versus outer */
   if(b1->strides[0] > b1->strides[1]) {
      f=1;  s=0;}
   else {
      f=0;  s=1;
   }
   if (verbose!=0){
        printf("Fast index is %d, slow index is %d, ",f,s);
        printf("strides[0]=%d, strides[1]=%d\n",b1->strides[0],b2->strides[1]);
   }

   /* Initialise a disjoint set in link 
    * image 1 has peak[i]=i ; i=1->n1
    * image 2 has peak[i]=i+n1 ; i=1->n2
    * link to hold 0->n1-1 ; n1->n2+n2-1 */
   safelyneed=n1+n2+1;
   res  = (PyArrayObject *)PyArray_FromDims(1,&safelyneed,PyArray_INT);
   link =  (int   *)   res->data;
   if(!res){  /* max length */
      PyErr_SetString(PyExc_ValueError,
                       "Failed to malloc link array");
      return NULL;
   }

   for(i=0;i<n1+n2+1;i++){link[i]=i;} /* first image */
   /* flag the start of image number 2 */
   link[n1]=-99999; /* Should never be touched by anyone */
   
  /* results lists of pairs of numbers */
   if(verbose!=0){
       printf("Scanning image, n1=%d n2=%d\n",n1,n2);
   }
   percent = b1->dimensions[s] / 100.;
   for( i = 0 ; i <= (b1->dimensions[s]-1) ; i++ ){    /* i,j is looping along the indices data array */
      if(verbose!=0 && (i%percent == 0) )printf(".");
      for( j = 0 ; j <= (b1->dimensions[f]-1) ; j++ ){
         p1=* (int *) (b1->data + i*b1->strides[s] + j*b1->strides[f]);
         if (p1==0){ continue; } 
         p2=* (int *) (b2->data + i*b2->strides[s] + j*b2->strides[f]);
         if (p2==0){ continue; } 
         /* Link contains the peak that this peak is */
	 if(link[p1]<0 || link[p2+n1]<0){
	   printf("Whoops p1=%d p2=%d p2+n1=%d link[p1]=%d link[p2+n1]=%d",p1,p2,p2+n1,link[p1],link[p2+n1]);
	   return NULL;
	 }
         /* printf("link[60]=%d %d %d ",link[60],p1,p2+n1);*/
         dset_makeunion(link,p1,p2+n1);
	 /* printf("link[60]=%d ",link[60]); */
	 if(verbose>2)printf("link[p1=%d]=%d link[p2+n1=%d]=%d\n",p1,link[p1],p2+n1,link[p2+n1]);
      } /* j */
   } /* i */
   if(verbose!=0){
      printf("\n");
      for(i=0;i<n1+n2;i++){
          if(i!=link[i]){
             printf("Link found!! %d %d\n",i,link[i]);
          }
      }
   }
   return Py_BuildValue("O", PyArray_Return(res));
}

static char update_blobs_doc[] =\
"   update_blobs (   Numeric.array(blob, 2D, Int), \n"\
"                    Numeric.array(set  , 2D, Int) , verbose=0) \n"\
" \n"\
" updates blob image such that : \n"\
" if blob[i,j] > 0: \n"\
"     blob[i,j] = set[blob[i,j]]\n"\
"\n"\
" Used to update a blob image merging peaks which have overlapped due\n"
" to the third dimension.\n";


static PyObject * update_blobs (PyObject *self, PyObject *args,  PyObject *keywds)
{
   PyArrayObject *bl=NULL,*set=NULL; /* in  */
   int i,j,s,f, verbose = 0, p1, v ; /* loop vars, flag */
   static char *kwlist[] = {"blobim","set","verbose", NULL};
   if(!PyArg_ParseTupleAndKeywords(args,keywds, "O!O!|i",kwlist,      
                        &PyArray_Type, &bl,   /* blobs */
                        &PyArray_Type, &set,   /* disjoint set array */
                        &verbose))        /* threshold and optional verbosity */
      return NULL;
   if(verbose!=0)printf("Welcome to update blobs\n");
   /* Check and validate args */
   if(bl->nd != 2 && bl->descr->type_num != PyArray_INT){     
      PyErr_SetString(PyExc_ValueError,
                       "Blob array must be 2d and integer, first arg problem");
      return NULL;
      }
   if(set->nd != 1 && bl->descr->type_num != PyArray_INT){     
      PyErr_SetString(PyExc_ValueError,
                       "Set array must be 1d and integer, second arg problem");
      return NULL;
      }
   /* Decide on fast/slow loop - inner versus outer */
   if(bl->strides[0] > bl->strides[1]) {
      f=1;  s=0;}
   else {
      f=0;  s=1;
   }
   if (verbose!=0){
        printf("Fast index is %d, slow index is %d, ",f,s);
        printf("strides[0]=%d, strides[1]=%d\n",bl->strides[0],bl->strides[1]);
   }
   for( i = 0 ; i <= (bl->dimensions[s]-1) ; i++ ){    /* i,j is looping along the indices data array */
      for( j = 0 ; j <= (bl->dimensions[f]-1) ; j++ ){
         p1 = * (int *) (bl->data + i*bl->strides[s] + j*bl->strides[f]);
         if (p1==0){ continue; }
         if (p1 < 0){
     		PyErr_SetString(PyExc_ValueError,
                       "Blob image contains negative number! Not allowed");
                       return NULL;
         }
         if (p1 < set->dimensions[0]){
         	/* Write in the value */
         	v = *(int *)(set->data + p1*set->strides[0]);
         	if (v > 0 && v < set->dimensions[0]){
           	     (* (int *)(bl->data + i*bl->strides[s] + j*bl->strides[f])) =  p1;
         	} else {
       		      PyErr_SetString(PyExc_ValueError,
                       "Set contains a bad value");
                       return NULL;
         	}         		
         }else{
      		PyErr_SetString(PyExc_ValueError,
                       "Blob image references overflows set array element");
                       return NULL;
         }
      }
   }
/* http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/52309 */
/* return None - side effect was on arg */
    Py_INCREF(Py_None);
    return Py_None;
}
	






static PyMethodDef connectedpixelsMethods[] = {
   {"connectedpixels", (PyCFunction) connectedpixels, METH_VARARGS | METH_KEYWORDS,
     connectedpixels_doc},
   {"blobproperties", (PyCFunction) blobproperties, METH_VARARGS | METH_KEYWORDS,
     blobproperties_doc},
   {"bloboverlaps", (PyCFunction) bloboverlaps,  METH_VARARGS | METH_KEYWORDS,
     bloboverlaps_doc},     
   {"update_blobs", (PyCFunction) update_blobs,  METH_VARARGS | METH_KEYWORDS,
     update_blobs_doc},     
   {"roisum", (PyCFunction) roisum, METH_VARARGS | METH_KEYWORDS,
     roisum_doc},
   {NULL, NULL, 0, NULL} /* setinel */
};

void 
initconnectedpixels(void)
{
   PyObject *m, *d, *s;

   m=Py_InitModule("connectedpixels", connectedpixelsMethods);
   import_array();
   d=PyModule_GetDict(m);
   s=PyString_FromString(moduledocs);
   PyDict_SetItemString(d,"__doc__",s);
   Py_DECREF(s);
   if(PyErr_Occurred())
     Py_FatalError("cant initialise connectedpixels");
}
