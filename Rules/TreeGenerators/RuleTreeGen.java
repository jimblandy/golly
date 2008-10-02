import java.util.* ;
public class RuleTreeGen {
   /* Put your state count, neighbor count, and function here */
   final static int numStates = 2 ;
   final static int numNeighbors = 8 ;
   /* order for nine neighbors is nw, ne, sw, se, n, w, e, s, c */
   /* order for five neighbors is n, w, e, s, c */
   int f(int[] a) {
      int n = a[0] + a[1] + a[2] + a[3] + a[4] + a[5] + a[6] + a[7] ;
      if (n == 2 && a[8] != 0)
         return 1 ;
      if (n == 3)
         return 1 ;
      return 0 ;
   }
   final static int numParams = numNeighbors + 1 ;
   HashMap<String, Integer> world = new HashMap<String, Integer>() ;
   ArrayList<String> r = new ArrayList<String>() ;
   int[] params = new int[numParams] ;
   int nodeSeq = 0 ;
   int getNode(String n) {
      Integer found = world.get(n) ;
      if (found == null) {
         found = nodeSeq++ ;
         r.add(n) ;
         world.put(n, found) ;
      }
      return found ;
   }
   int recur(int at) {
      if (at == 0)
         return f(params) ;
      String n = "" + at ;
      for (int i=0; i<numStates; i++) {
         params[numParams-at] = i ;
         n += " " + recur(at-1) ;
      }
      return getNode(n) ;
   }
   void writeRuleTree() {
      System.out.println("num_states=" + numStates) ;
      System.out.println("num_neighbors=" + numNeighbors) ;
      System.out.println("num_nodes=" + r.size()) ;
      for (int i=0; i<r.size(); i++)
         System.out.println(r.get(i)) ;
   }
   public static void main(String[] args) throws Exception {
      RuleTreeGen rtg = new RuleTreeGen() ;
      rtg.recur(numParams) ;
      rtg.writeRuleTree() ;
   }
}
