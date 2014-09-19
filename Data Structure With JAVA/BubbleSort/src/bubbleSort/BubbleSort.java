package bubbleSort;

public class BubbleSort {
	public static void sort(int[] a)
	{
		// POSTCONDITION: a[0] <= a[1] <= ... <= a[a.length-1];
		for (int i = a.length-1; i > 0; i--) 
		{ // step 1
			for (int j = 0; j < i; j++) 
			{ // step 2
				if (a[j] > a[j+1]) 
				{
					swap(a, j, j+1); // step 3
				}
			}
			// INVARIANTS: a[i] <= a[i+1] <= ... <= a[a.length-1];
			// a[j] <= a[i] for all j < i;
			System.out.printf("%d:",i);
			print(a);
		}
	}
	
	private static void swap(int[] a, int i, int j) 
	{
		// PRECONDITIONS: 0 <= i < a.length; 0 <= j < a.length; 
		// POSTCONDITION: a[i] and a[j] are interchanged;
		if (i == j) 
		{
			return;
		}
		int temp=a[j];
		a[j] = a[i];
		a[i] = temp;
	}
	
	private static void print(int[] a) 
	{
		for (int ai : a) 
		{
			System.out.printf("%s ", ai);
		}
		System.out.println();
	}
	
	public static void main(String[] args)
	{
		int[] a = {77, 44, 99, 66, 33, 55, 88, 22};
		sort(a);
	}
}
