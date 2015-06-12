package mynumber1;

import java.util.*;

class MyNumber1 {
	int primary, secondary;
	MyNumber1(int primary, int secondary) {
		this.primary = primary;
		this.secondary = secondary;
	}
	
	public int hashCode() {
		//return secondary;
		return primary;
	}
	
	public boolean equals(Object o) {
		if (o != null && o instanceof MyNumber1)
			return (primary == ((MyNumber1)o).primary);
			//return (primary == ((MyNumber1)o).primary) && (secondary == ((MyNumber1)o).secondary);
		else
			return false;
	}
	
	public static void main(String args[]) {
		Map<MyNumber1, String> map = new HashMap<>();
		MyNumber1 num1 = new MyNumber1(2500, 100);
		MyNumber1 num2 = new MyNumber1(2500, 200);
		
		System.out.println(num1.equals(num2));
		
		map.put(num1, "Shreya");
		System.out.println(map.get(num1));
		System.out.println(map.get(num2));
	}
}
