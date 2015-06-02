package eJavaGuruArray;

class EJavaGuruArray {
	public static void main(String args[]) {
		int[] arr = new int[5];
		byte b = 4; char c = 'c'; long longVar = 10;
		arr[0] = b;
		arr[1] = c;
		arr[3] = (int) longVar;
		System.out.println(arr[0]);
		System.out.println((char)arr[1]);
		System.out.println(arr[2]);
		System.out.println(arr[3]);
		
		System.out.println(arr[0] + arr[1] + arr[2] + arr[3]);
	}
}
