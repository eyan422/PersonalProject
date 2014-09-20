package insertionSort;

public class InsertionSort {
	
	public static void sort(int[] a)
	{
		int ai = 0;
		int i,j;
		
		for (i = 1; i < a.length; i++) 
		{ 
			ai = a[i];
			for (j = i; j > 0 && a[j-1] > ai; j--) 
			{
				a[j] = a[j-1];
			}
			
			//System.out.printf("%d:",i);
			//print(a);
			
			a[j] = ai;
			
			System.out.printf("%d:",i);
			print(a);
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
	
	public static void main(String[] args)
	{
		int[] a = {3,2,1,0};
		System.out.printf("0:");
		print(a);
		sort(a);
	}
}
