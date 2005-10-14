#!/usr/bin/perl
@files = glob "*.htm" ;
for $file (@files) {
   open F, "$file" or die "Can't open $file\n" ;
   @f = <F> ;
   close F ;
   for (@f) {
      s/<body>/<body bgcolor="#FFFFCE"><font size=-1>/ ;
      s!<center><a HREF="http://www.argentum.freeserve.co.uk/lex_home.htm">Life Lexicon Home Page</a>!<center><A HREF="lex.htm">Introduction</A> \| <A HREF="lex_bib.htm">Bibliography</A></center>! ;
      s|<center><b class=l>Life Lexicon</b></center>|| ;
      s:^<A HREF="lex.htm">Introduction</A> \|:<font size=-1><b>: ;
      s:^<a href="lex.htm">Introduction</a> \|:<font size=-1><b>: ;
      s:<A HREF="lex_z.htm">Z</A> \|:<A href="lex_z.htm">Z</A></b></font>: ;
      s:<a href="lex_z.htm">Z</a> \|:<A href="lex_z.htm">Z</A></b></font>: ;
      s|^<A HREF="lex_bib.htm">Bibliography</A>|| ;
      s|^<a href="lex_bib.htm">Bibliography</a>|| ;
      s|<h3|<h5| ; s|</h3|</h5| ;
      s|<H3|<h5| ; s|</H3|</h5| ;
      s/^\t*// ;
      s/&times;/x/g ;
      s/&oacute;/o/g ;
      s|^<pre>|<center><table cellspacing=0 cellpadding=0><tr><td><pre><a href="lexpatt:">| ;
      s|^</pre>|</a></pre></td></tr></table></center>| ;
      s|^</BODY>|</font></body>| ;
      s|^</body>|</font></body>| ;
      # fix problems in lex_[hps].htm
      s|<a href="lexpatt:">\n-|<font size=-1>\n-| ; s|-\n</a>|-\n</font>| ;
      # fix problem in lex_l.htm
      s|<a href="lexpatt:">\nMar|<font size=-1>\nMar| ; s|1973\n</a>|1973\n</font>| ;
   }
   open F, ">$file" or die "Can't write $file\n" ;
   print F @f ;
   close F ;
}
