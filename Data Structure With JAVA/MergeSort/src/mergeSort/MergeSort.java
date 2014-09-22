package mergeSort;

public class MergeSort 
{
	public static void main(String[] arg)
	{
		int[] a = {77, 44, 99, 66, 33, 55, 88, 22};
		System.out.println("length:"+a.length);
		sort(a);
		print(a);
	}
	
	public static void sort(int[] a)
	{
		sort(a,0,a.length);
	}
	
	private static void sort(int[]a, int p,int q)
	{
		int m = 0;
		if(q - p < 2)
		{
			return;
		}
		
		m = (p + q) / 2;
		sort(a,p,m);
		sort(a,m,q);
		merge(a,p,m,q);
	}
	
	private static void merge(int[] a, int p, int m, int q)
	{
		if(a[m-1] < a[m])
		{
			return;
		}
		
		int i = p, j = m, k = 0;
		int [] tmp = new int[q-p];
		while(i < m && j < q)
		{
			tmp[k++] = a[i] < a[j] ? a[i++] : a[j++];
		}
		System.arraycopy(a,i,a,p+k,m-i);
		System.arraycopy(tmp,0,a,p,k);
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
