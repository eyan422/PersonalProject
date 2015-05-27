package myphone;

class MyPhone {
	static boolean softKeyboard = true;
	static String phoneNumber = "1";
	
	static void myMethod() {
		boolean softKeyboard = true;
		String phoneNumber = "2";
		
		System.out.println(phoneNumber);
	}
	
	public static void main(String... args)
	{
		myMethod();
		System.out.println(phoneNumber);
	}
}
