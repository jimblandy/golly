# Set the states, neighbors, and function here.
my $numStates = 2 ;
my $numNeighbors = 8 ;
# order for nine neighbors is nw, ne, sw, se, n, w, e, s, c
# order for five neighbors is n, w, e, s, c
sub f {
   my ($nw, $ne, $sw, $se, $n, $w, $e, $s, $c) = @_ ;
   my $sum = $nw + $ne + $sw + $se + $n + $w + $e + $s ;
   return ($sum == 3 || ($sum == 2 && $c)) ? 1 : 0 ;
}
my $numParams = $numNeighbors + 1 ;
my %world ;
my $nodeseq = 0 ;
my @r ;
my @params = (0) x $numParams ;
sub getNode {
   my $n = shift ;
   return $world{$n} if defined($world{$n}) ;
   $world{$n} = $nodeseq ;
   push @r, $n ;
   return $nodeseq++ ;
}
sub recur {
   my $at = shift ;
   if ($at == 0) {
      return f(@params) ;
   } else {
      my $n = $at ;
      for (my $i=0; $i<$numStates; $i++) {
         $params[$numParams-$at] = $i ;
         $n .= " " . recur($at-1) ;
      }
      return getNode($n) ;
   }
}
recur($numParams) ;
print "num_states=$numStates\n" ;
print "num_neighbors=$numNeighbors\n" ;
print "num_nodes=", scalar @r, "\n" ;
print "$_\n" for @r ;
