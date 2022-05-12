#!/usr/bin/perl
@files = glob "*.htm" ;
for $file (@files) {
   open F, "$file" or die "Can't open $file\n" ;
   @f = <F> ;
   close F ;
   $f = join('', @f) ;
   for (($f)) {
      s/<body>/<body bgcolor="#FFFFCE">/g ;
      s|<img src="./at.gif" alt="@">|@|g ;
      s!<center><a HREF="https://conwaylife.com/ref/lexicon/lex_home.htm">Life Lexicon Home Page</a>!<center><A HREF="lex.htm">Introduction</A> \| <A HREF="lex_bib.htm">Bibliography</A></center>!g ;
      s!<center><a HREF="lex_home.htm">Life Lexicon Home Page</a>!<center><A HREF="lex.htm">Introduction</A> \| <A HREF="lex_bib.htm">Bibliography</A></center>!g ;
      s|<center><b class=l>Life Lexicon</b></center>||g ;
      s:^<A HREF="lex.htm">Introduction</A> \|:<font size=-1><b>:gm ;
      s:^<a href="lex.htm">Introduction</a> \|:<font size=-1><b>:gm ;
      s:<A HREF="lex_z.htm">Z</A> \|:<A href="lex_z.htm">Z</A></b></font>:g ;
      s:<a href="lex_z.htm">Z</a> \|:<A href="lex_z.htm">Z</A></b></font>:g ;
      s|^<A HREF="lex_bib.htm">Bibliography</A>||gm ;
      s|^<a href="lex_bib.htm">Bibliography</a>||gm ;
      s|<h3|<h5|g ; s|</h3|</h5|g ;
      s|<H3|<h5|g ; s|</H3|</h5|g ;
      s/^\t*//gm ;
      s/&times;/x/g ;
      s/&oacute;/o/g ;
      s|^<pre>|<center><table cellspacing=0 cellpadding=0><tr><td><pre><a href="lexpatt:">|gm ;
      s|^</pre>|</a></pre></td></tr></table></center>|gm ;
      # fix problems in lex_[hps].htm
      s|<a href="lexpatt:">\n-|<font size=-1>\n-|g ; s|-\n</a>|-\n</font>|g ;
      # fix problem in lex_l.htm
      s|<a href="lexpatt:">\nMar|<font size=-1>\nMar|g ; s|1973\n</a>|1973\n</font>|g ;
   }
   open F, ">$file" or die "Can't write $file\n" ;
   print F $f ;
   close F ;
}
