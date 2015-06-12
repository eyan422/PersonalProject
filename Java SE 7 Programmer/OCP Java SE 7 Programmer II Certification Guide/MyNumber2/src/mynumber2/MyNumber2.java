package mynumber2;

import java.util.*;

class MyNumber2 {
	int number;
	MyNumber2(int number) {this.number = number;}
		public int hashCode() {
			return ((int)(Math.random() * 100));
	}
	
	public boolean equals(Object o) {
		if (o != null && o instanceof MyNumber2)
			return (number == ((MyNumber2)o).number);
		else
			return false;
	}
	
	public static void main(String args[]) {
		Map<MyNumber2, String> map = new HashMap<>();
		MyNumber2 num1 = new MyNumber2(2500);
		map.put(num1, "Shreya");
		System.out.println(map.get(num1));
	}
}