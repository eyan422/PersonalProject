package shellSort;

public class ShellSort {
	
	public static void main(String[] args)
	{
		int[] a = {77, 44, 99, 66, 33, 55, 88, 22};
		sort(a);
		print(a);
	}
	
	public static void sort(int[] a) 
	{
		// POSTCONDITION: a[0] <= a[1] <= ... <= a[a.length-1];
		
		// step 1
		int d = 1, j, n = a.length; 
		
		while (9*d < n) 
		{ // step 2
			d = 3*d + 1; // step 3
		}
		
		while (d > 0) 
		{ // step 4
			//InsertionSort
			for (int i = d; i < n; i++) 
			{ // step 5
				int ai = a[i];
				j = i;
				while (j >= d && a[j-d] > ai) 
				{
					a[j] = a[j-d];
					j -= d;
				}
				a[j] = ai;
			}
			d /= 3; // step 6
		}
	}
	
	private static void print(int[] a) 
	{
		for (int ai : a) 
		{
			System.out.printf("%s ", ai);
		}
		System.out.println();
	}
}
