# Fix lexicon .htm files included with iOS/Android/web Golly.
# 
# We replace this text:
#
# <center><table cellspacing=0 cellpadding=0><tr><td><pre><a href="lexpatt:">
# .O.....
# ...O...
# OO..OOO
# </a></pre>etc...
#
# with:
#
# <center><table cellspacing=0 cellpadding=0><tr><td><pre><a href="lexpatt:.O.....$...O...$OO..OOO$">
# .O.....
# ...O...
# OO..OOO
# </a></pre></td></tr></table></center>
#
# We also increase font size used for links at top and bottom of each page.

import re

# ------------------------------------------------------------------------------

def fix_file(filename):
    print 'fixing ' + filename
    f = open(filename, 'r')
    contents = f.read()
    
    # use re.DOTALL to include newlines in match
    for pattdata in re.findall('<a href="lexpatt:">\n(.*?)</a>', contents, re.DOTALL):
        newdata = pattdata.replace('\n','$')
        contents = contents.replace('<a href="lexpatt:">\n'+pattdata+'</a>',
                                    '<a href="lexpatt:'+newdata+'"\n>'+pattdata+'</a>', 1)
    
    # remove small font used for links at top and bottom of each page
    contents = contents.replace('<font size=-1><b>', '<b>', 2)
    contents = contents.replace('Z</A></b></font>', 'Z</A></b>', 2)
    
    f.close()
    
    f = open(filename, 'w')
    f.write(contents)
    f.close()

# ------------------------------------------------------------------------------

fix_file("lex.htm")
fix_file("lex_1.htm")
fix_file("lex_a.htm")
fix_file("lex_b.htm")
fix_file("lex_c.htm")
fix_file("lex_d.htm")
fix_file("lex_e.htm")
fix_file("lex_f.htm")
fix_file("lex_g.htm")
fix_file("lex_h.htm")
fix_file("lex_i.htm")
fix_file("lex_j.htm")
fix_file("lex_k.htm")
fix_file("lex_l.htm")
fix_file("lex_m.htm")
fix_file("lex_n.htm")
fix_file("lex_o.htm")
fix_file("lex_p.htm")
fix_file("lex_q.htm")
fix_file("lex_r.htm")
fix_file("lex_s.htm")
fix_file("lex_t.htm")
fix_file("lex_u.htm")
fix_file("lex_v.htm")
fix_file("lex_w.htm")
fix_file("lex_x.htm")
fix_file("lex_y.htm")
fix_file("lex_z.htm")
fix_file("lex_bib.htm")
