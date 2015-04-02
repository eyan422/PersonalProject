package mobile;

import java.lang.Object;

class Phone {
	static boolean softKeyboard = true;
}

class Mobile {
	public static void main(String[] args) {
		Phone.softKeyboard = false;
		
		Phone p1 = new Phone();
		Phone p2 = new Phone();
		
		System.out.println(p1.softKeyboard);
		System.out.println(p2.softKeyboard);
		
		p1.softKeyboard = true;
		
		System.out.println(p1.softKeyboard);
		System.out.println(p2.softKeyboard);
	}
}
