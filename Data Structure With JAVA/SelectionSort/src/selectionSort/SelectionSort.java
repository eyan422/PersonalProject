package selectionSort;

public class SelectionSort {
	public static void sort(int[] a)
	{
		int m = 0;
		// POSTCONDITION: a[0] <= a[1] <= ... <= a[a.length-1];
		for (int i = a.length-1; i > 0; i--) 
		{ // step 1
			m = 0;
			for (int j = 1; j < i; j++) 
			{ // step 2
				if (a[j] > a[m]) 
				{
					m = j;
				}
			}
			
			swap(a,i,m);
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
