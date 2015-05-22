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
	}
}
