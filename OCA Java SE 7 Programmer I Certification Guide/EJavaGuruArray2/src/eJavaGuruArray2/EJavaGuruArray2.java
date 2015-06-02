package eJavaGuruArray2;

class EJavaGuruArray2 {
	public static void main(String args[]) {
		char[] arr1;
		char[] arr2 = new char[3];
		char[] arr3 = {'a', 'b'};
		arr1 = arr2;
		arr1 = arr3;
		System.out.println(arr1[0] + ":" + arr1[1]);
		
		int[][] array = new int[][]{{1, 2, 3}, {}, {1, 2,3, 4, 5}};
	}
}
