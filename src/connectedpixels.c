



/*
# ImageD11_v1.x Software for beamline ID11
# Copyright (C) 2005-2017  Jon Wright
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


/* Updated to use f2py to generate bindings, 2017 */

static char moduledocs[] =\
"   C extensions for image analysis, part of ImageD11\n"\
"   ";


#include "blobs.h"   /* Disjoint sets thing for blob finding */

/* ==== connectedpixels ======================================== */

static char connectedpixels_doc[] =\
"   nblobs = connectedpixels ( data=Numeric.array(data, 2D)  , \n"\
"                              labels=Numeric.array(blob, 2D, Int)  ,\n"\
"                              threshold=float threshold ,\n"\
"                              verbose=Int verbose )\n"\
"   data is normally an image \n"\
"   blob is an array to receive pixel -> blob assignments\n"\
"   threshold is the value above which a pixel is considered to be in a blob\n"\
"   verbose flags printing on stdout";

                              
/* Fill in an image of peak assignments for pixels */


void match( int *new, int *old, int *S){
  /* printf("match %d %d\n",*new,*old); */
  if(*new == 0) { 
      *new = *old ; 
  } else {
    if ( *new != *old ) {
        dset_makeunion( S, *old, *new);
    }
  }
}


int connectedpixels( float* data, int* labels, float threshold, int verbose, 
        int ns, int nf, int eightconnected){

  int i, j, k, *S, *T, irp, ir, ipx, np;

  if( verbose ){ 
      printf("Welcome to connectedpixels ");
      if(eightconnected) printf("Using connectivity 8\n");
      else printf("Using connectivity 4\n");
  }

  /* lots of peaks possible */
  S = dset_initialise( 16384 ) ;

  /* To simplify later we hoist the first row and first pixel
   * out of the loops
   *
   * Algorithm scans image looking at stuff previously seen
   */

  /* First point */
  /*  i = 0;   j = 0; */
  if (data[0] > threshold) {
      S = dset_new( &S, &labels[0] );
  } else {
      labels[0] = 0;
  }
  /* First row */
  for (j = 1; j<nf; j++){
      labels[j] = 0; /* initialize */
      if(data[j] > threshold){
          if( labels[j-1] > 0) {
              labels[j] = labels[j-1];
          } else {
              S = dset_new( &S, &labels[j] );
          }
      }
  }

  /* === Mainloop =============================================*/
  for(i=1; i<ns;i++){ /* i-1 prev row always exists, see above */
      ir = i*nf;   /* this row */
      irp = ir-nf; /* prev row */
      /* First point */
      /* j=0; */
      labels[ir]=0;
      if( data[ir] > threshold ){
          if( labels[irp] > 0){
              labels[ir] = labels[irp];
          }
          if( eightconnected && ( labels[irp+1] > 0 ) ){
              match( &labels[ir], &labels[irp+1], S);
          }
          if( labels[ir] == 0 ){
              S = dset_new( &S, &labels[ir] );
          }
      }
      /* Run along row to just before end */
      for(j=1; j<nf-1; j++){
          ipx = ir+j;
          irp = ipx-nf;
          labels[ipx]=0;
          if( data[ipx] > threshold ){
              /* Pixel needs to be assigned */
              if( eightconnected && ( labels[irp-1] > 0 ) ){
                  match( &labels[ ipx ], &labels[irp-1], S);
              }
              if( labels[irp] > 0 ){
                  match( &labels[ ipx ], &labels[irp], S);
              }
              if( eightconnected && ( labels[irp+1] > 0 )){
                  match( &labels[ ipx ], &labels[irp+1], S);
              }
              if( labels[ipx-1] > 0){
                  match( &labels[ ipx ], &labels[ipx-1], S);
              }
              if ( labels[ipx] == 0 ){ /* Label is new ! */
                  S = dset_new( &S, &labels[ipx] );
              }
          } /* (val > threshold) */
      } /* Mainloop j */
      /* Last pixel on the row */
      ipx++;  
      irp++;
      labels[ipx]=0;
      if( data[ipx] > threshold ){
              if( eightconnected && ( labels[irp-1] > 0 ) ){
                  match( &labels[ ipx ], &labels[irp-1], S);
              }
              if( labels[irp] > 0 ){
                  match( &labels[ ipx ], &labels[irp], S);
              }
              if( labels[ipx-1] > 0){
                  match( &labels[ ipx ], &labels[ipx-1], S);
              }
              if ( labels[ipx] == 0 ){ /* Label is new ! */
                  S = dset_new( &S, &labels[ipx] );
              }
      }




  }
  /* Now compress the disjoint set to make single list of 
   * unique labels going from 1->n
   */
  T = dset_compress( &S, &np );
  /* Now scan through image re-assigning labels as needed */
  for(i=0;i<ns;i++){
      for(j=0;j<nf;j++){
          ipx = i*nf + j;
          k = labels[ipx];
          if( k > 0 ){
             if( T[k] == 0 ) {
                 printf("Error in connectedpixels\n");
             }
             if( T[k] != k ) { 
                 labels[i*nf+j] = T[k];
             }
          }
      }
  }
  free(S);
  free(T);
  return np;
}



