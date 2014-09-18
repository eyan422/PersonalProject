package arraySort;

public class ArraySort {
		public static void main(String[] args)
		{
			int[] a = {77, 44, 99, 66, 33, 55, 88, 22};
			print(a);
			java.util.Arrays.sort(a);
			print(a);
		}
		
		private static void print(int[] a) 
		{
			for (int ai : a) 
			{
				System.out.printf("%s ", ai);
			}
			System.out.println();
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
}


