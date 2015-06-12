package mynumber3;

import java.util.*;

class MyNumber {
	int number;
	MyNumber(int number) {this.number = number;}
	
	public int hashCode() {
		return number;
	}
	
	public boolean equals(Object o) {
		if (o != null && o instanceof MyNumber)
			return (number == ((MyNumber)o).number);
		else
			return false;
	}
	
	public static void main(String args[]) {
		Map<MyNumber, String> map = new HashMap<>();
		MyNumber num1 = new MyNumber(2500);
		map.put(num1, "Shreya");
		num1.number = 100;
		System.out.println(map.get(num1));
	}
}