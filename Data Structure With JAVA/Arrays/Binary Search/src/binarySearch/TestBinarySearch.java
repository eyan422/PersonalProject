package binarySearch;

public class TestBinarySearch {
	public static void main(String[] args) {
		int[] a = {22, 33, 44, 55, 66, 77, 88, 99};
		DuplicatingArrays.print(a);
		
		
		System.out.println("search(a, 44): "  + search(a, 44));
		System.out.println("search(a, 50): "  + search(a, 50));
		System.out.println("search(a, 77): "  + search(a, 77));
		System.out.println("search(a, 100): " + search(a, 100));
		
		System.out.println("+++++++++++++++++++++++");
		
		System.out.println("BinarySearch(a, 44): "  + BinarySearch(a, 44, 0,a.length));
		System.out.println("BinarySearch(a, 50): "  + BinarySearch(a, 50, 0,a.length));
		System.out.println("BinarySearch(a, 77): "  + BinarySearch(a, 77, 0,a.length));
		System.out.println("BinarySearch(a, 100): " + BinarySearch(a, 100, 0,a.length));
	}
	
	public static int search(int[] a, int x) {
		
		// POSTCONDITIONS: returns i; 
		// if i >= 0, then a[i] == x; otherwise i == -1;
		int lo = 0;
		int hi = a.length;
		
		while (lo < hi) { // step 1
			
			// INVARIANT: if a[j]==x then lo <= j < hi; // step 3
			int i = (lo + hi) / 2; // step 4
			
			//System.out.println("i = " + i);
			
			if (a[i] == x) {
				return i; // step 5
			} else if (a[i] < x) {
				lo = i+1; // step 6	
			} else {
				hi = i; // step 7
			}
		}
		return -1; // step 2
	}
	
	public static int BinarySearch(int[] a, int x, int lo, int hi) {
		int i = (lo + hi) / 2;
		
		if (lo >= hi)
			return -1;
		
		if (a[i] == x) {
			return i; // step 5
		} else if (a[i] < x) {
			//lo = i+1; // step 6
			return BinarySearch(a,x,i+1,hi);
		} else {
			//hi = i; // step 7
			return BinarySearch(a,x,lo,i);
		}
		
	}
}