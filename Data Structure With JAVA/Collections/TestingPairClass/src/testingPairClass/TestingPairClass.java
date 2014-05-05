package testingPairClass;

//import java.util.*;

 public class TestingPairClass {
	 public static void main(String[] args) {
		 
	 Pair<Month, Integer> christmas = new Pair<Month,Integer>(Month.DEC, 25);
	 System.out.println(christmas);
	 
	 Month month = christmas.getFirst();
	 
	 int day = christmas.getSecond();
	 System.out.printf("%d-%s%n", day, month);
	 }
 }

 enum Month { JAN, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCT, NOV, DEC }

 class Pair<S, T> {
	 private S first;
	 private T second;
	
	 public Pair(S first, T second) {
	 this.first = first;
	 this.second = second;
	 }
	
	 public S getFirst() {
	 return first;
	 }
	
	 public T getSecond() {
	 return second;
	 }
	
	 public String toString() {
	 return "(" + first + ", " + second + ")";
	 }
 }
