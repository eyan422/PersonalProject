package testTreeSet;

import java.util.*;

public class TestTreeSet {
 public static void main(String[] args) {
 NavigableSet<String> engl = new TreeSet<String>(); // Without an explicit comparator
 
 Collections.addAll(engl, "IN", "US", "PK", "NG", "PH", "GB", "ZA");
 System.out.println(engl);
 
 engl.add("KE");
 System.out.println(engl);
 
 /*
  * The headSet()method returns the sorted subset of all elements that precede its
  * argument. The subSet()method returns the sorted subset of elements that begin with its first argument
  * and precede its second argument. The tailSet()method returns the sorted subset of all elements that do
  * not precede its argument.
  * */
 SortedSet<String> head = engl.headSet("KE");
 SortedSet<String> mid = engl.subSet("KE", "US");
 SortedSet<String> tail = engl.tailSet("US");
 
 System.out.printf("%s %s %s%n", head, mid, tail);
 
 System.out.printf("engl.first(): %s%n", engl.first());
 System.out.printf("engl.last(): %s%n", engl.last());
 }
}
