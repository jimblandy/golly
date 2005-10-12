#!/bin/perl
@files = glob "*.htm" ;
for $file (@files) {
   open F, "$file" or die "Can't open $file\n" ;
   @f = <F> ;
   close F ;
   for (@f) {
      s/<body>/<body bgcolor="#FFFFCE"><font size=-1>/ ;
      s/^\t*// ;
      s/&times;/x/g ;
   }
   open F, ">$file" or die "Can't write $file\n" ;
   print F @f ;
   close F ;
}
