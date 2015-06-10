package arrayinit;

public class ArrayInit {
	@SuppressWarnings("unused")
	public static void main(String args[])
	{
		int eArr1[] = {10, 23, 10, 2};
		int[] eArr2 = new int[10];
		int[] eArr3 = new int[] {};
		
		/* Error Initialization
		int[] eArr4 = new int[10] {};
		int eArr5[] = new int[2] {10, 20};
		*/
		int eArr5[] = new int[] {10, 20};
		
		int[][] array1 = {{1, 2, 3}, {}, {1, 2,3, 4, 5}};
		//int[][] array2 = new array() {{1, 2, 3}, {}, {1, 2,3, 4, 5}};
		//int[][] array3 = {1, 2, 3}, {0}, {1, 2,3, 4, 5};
		int[][] array5 = new int[2][];
	}
}
