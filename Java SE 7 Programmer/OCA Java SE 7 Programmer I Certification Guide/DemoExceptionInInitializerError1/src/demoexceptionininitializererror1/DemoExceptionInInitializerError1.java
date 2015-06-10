package demoexceptionininitializererror1;

public class DemoExceptionInInitializerError1 {
	static String name = null;
	static int nameLength = name.length();
	
	public static void main(String args[])
	{
		System.out.println("exception");
	}
}
