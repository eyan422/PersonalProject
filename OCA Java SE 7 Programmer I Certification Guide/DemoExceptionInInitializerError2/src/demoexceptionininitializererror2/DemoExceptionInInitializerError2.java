package demoexceptionininitializererror2;

@SuppressWarnings("serial")
class MyException extends RuntimeException{}

public class DemoExceptionInInitializerError2 {
	static String name = getName();
	
	public static void main(String args[])
	{
		
	}
	
	static String getName() {
		throw new MyException();
	}
}