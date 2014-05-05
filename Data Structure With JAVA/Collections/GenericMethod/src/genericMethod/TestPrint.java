package genericMethod;

import java.util.*;

public class TestPrint {
	public static void main(String[] args) {
		args = new String[]{"CA", "US", "MX", "HN", "GT"};
		print(args);
		//ptintUniversal(args);
		
		/*
		int[] m = {1,2,3,4};
		print(m);
		*/
	}

	static <E> void print(E[] a) {
		for (E ae : a) {
			System.out.printf("%s ", ae);
		}
		System.out.println();
	}
	
	static void ptintUniversal(Collection<?> c) {
		for (Object o:c){
			System.out.printf("%s ", o);
		}
		System.out.println();
	}
}
